#include "ltfa/cut_analysis.hpp"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>

namespace ltfa {
namespace {

struct Edge {
    std::size_t neighbor{0};
    RelationId relation;
};

struct ViewGraph {
    AnalysisStatus status{AnalysisStatus::invalid};
    std::vector<bool> selected_objects;
    std::vector<bool> selected_relations;
    std::vector<std::vector<Edge>> adjacency;
    AssuranceQualification qualification;
    bool contains_unknown{false};
};

struct Exclusions {
    std::vector<bool> objects;
    std::vector<bool> relations;
};

struct Reachability {
    AnalysisStatus status{AnalysisStatus::invalid};
    bool reachable{false};
};

struct FlowEdge {
    std::size_t to{0};
    std::size_t reverse{0};
    std::size_t capacity{0};
};

[[nodiscard]] std::size_t object_count(const NormalizedModel &model) noexcept {
    return model.entities.size() + model.endpoints.size();
}

[[nodiscard]] bool valid_object(const NormalizedModel &model, ObjectRef object) noexcept {
    return object.is_valid() &&
           (object.kind == ObjectKind::entity ? object.index < model.entities.size()
                                              : object.index < model.endpoints.size());
}

[[nodiscard]] std::size_t ordinal(const NormalizedModel &model, ObjectRef object) noexcept {
    return object.kind == ObjectKind::entity ? object.index : model.entities.size() + object.index;
}

[[nodiscard]] ObjectRef object_from_ordinal(const NormalizedModel &model,
                                            std::size_t value) noexcept {
    if (value < model.entities.size()) {
        return ObjectRef::from(EntityId{static_cast<std::uint32_t>(value)});
    }
    return ObjectRef::from(EndpointId{static_cast<std::uint32_t>(value - model.entities.size())});
}

[[nodiscard]] ScopeId object_scope(const NormalizedModel &model, ObjectRef object) noexcept {
    return object.kind == ObjectKind::entity ? model.entities[object.index].scope
                                             : model.endpoints[object.index].scope;
}

[[nodiscard]] EntityId owner(const NormalizedModel &model, ObjectRef object) noexcept {
    return object.kind == ObjectKind::entity ? EntityId{object.index}
                                             : model.endpoints[object.index].owner;
}

void combine_qualification(AssuranceQualification &aggregate,
                           AssuranceQualification value) noexcept {
    if (value.completeness == Completeness::unknown) {
        aggregate.completeness = Completeness::unknown;
    } else if (value.completeness == Completeness::partial &&
               aggregate.completeness == Completeness::verified) {
        aggregate.completeness = Completeness::partial;
    }
    if (value.evidence_quality == EvidenceQuality::unknown) {
        aggregate.evidence_quality = EvidenceQuality::unknown;
    } else if (value.evidence_quality == EvidenceQuality::inferred &&
               aggregate.evidence_quality == EvidenceQuality::confirmed) {
        aggregate.evidence_quality = EvidenceQuality::inferred;
    }
}

[[nodiscard]] bool selected_by_filter(const NormalizedModel &model,
                                      const AvailabilitySnapshot &snapshot, ObjectRef object,
                                      const GraphFilter &filter,
                                      const std::vector<bool> &domain_mask) noexcept {
    const auto entity = owner(model, object);
    const auto scope = object_scope(model, object);
    if (!entity.is_valid() || entity.value() >= model.entities.size() || !scope.is_valid() ||
        scope.value() >= model.scopes.size()) {
        return false;
    }
    const auto &entity_value = model.entities[entity.value()];
    return (!filter.plane.has_value() || entity_value.plane == *filter.plane) &&
           (!filter.layer.has_value() || entity_value.layer == *filter.layer) &&
           (!filter.scope.has_value() || scope == *filter.scope) &&
           (!filter.failure_domain.has_value() || domain_mask[ordinal(model, object)]) &&
           filter.availability.includes(snapshot.state(object));
}

[[nodiscard]] bool selected_relation_kind(const GraphFilter &filter, RelationKind kind) noexcept {
    return filter.relation_kinds.empty() ||
           std::find(filter.relation_kinds.begin(), filter.relation_kinds.end(), kind) !=
               filter.relation_kinds.end();
}

void add_arc(ViewGraph &graph, const NormalizedModel &model, ObjectRef from, ObjectRef to,
             RelationId relation) {
    graph.adjacency[ordinal(model, from)].push_back({ordinal(model, to), relation});
}

[[nodiscard]] ViewGraph build_view(const NormalizedModel &model,
                                   const AvailabilitySnapshot &snapshot,
                                   const GraphFilter &filter) {
    ViewGraph graph;
    const auto count = object_count(model);
    graph.selected_objects.assign(count, false);
    graph.selected_relations.assign(model.relations.size(), false);
    graph.adjacency.resize(count);
    graph.qualification = {Completeness::verified, EvidenceQuality::confirmed};
    if (snapshot.entity_states.size() != model.entities.size() ||
        snapshot.endpoint_states.size() != model.endpoints.size() ||
        (filter.scope.has_value() &&
         (!filter.scope->is_valid() || filter.scope->value() >= model.scopes.size()))) {
        return graph;
    }
    std::vector<bool> domain_mask(count, true);
    if (filter.failure_domain.has_value()) {
        std::fill(domain_mask.begin(), domain_mask.end(), false);
        const auto expansion = expand_failure_domain(model, *filter.failure_domain);
        if (!expansion.has_value()) {
            return graph;
        }
        for (const auto member : expansion->members) {
            domain_mask[ordinal(model, member)] = true;
        }
    }
    for (std::size_t value = 0; value < count; ++value) {
        const auto object = object_from_ordinal(model, value);
        if (!selected_by_filter(model, snapshot, object, filter, domain_mask)) {
            continue;
        }
        graph.selected_objects[value] = true;
        const auto scope = object_scope(model, object);
        const auto &scope_value = model.scopes[scope.value()];
        combine_qualification(graph.qualification,
                              {scope_value.completeness, scope_value.evidence_quality});
        graph.contains_unknown =
            graph.contains_unknown || snapshot.state(object) == AvailabilityState::unknown;
    }
    for (const auto &relation : model.relations) {
        if (!relation.id.is_valid() || relation.id.value() >= model.relations.size() ||
            !valid_object(model, relation.from) || !valid_object(model, relation.to) ||
            !selected_relation_kind(filter, relation.kind) ||
            !graph.selected_objects[ordinal(model, relation.from)] ||
            !graph.selected_objects[ordinal(model, relation.to)]) {
            continue;
        }
        graph.selected_relations[relation.id.value()] = true;
        const bool bidirectional = relation.direction == Direction::bidirectional;
        const bool forward = bidirectional || filter.direction != TraversalDirection::reverse;
        const bool reverse = bidirectional || filter.direction != TraversalDirection::forward;
        if (forward) {
            add_arc(graph, model, relation.from, relation.to, relation.id);
        }
        if (reverse) {
            add_arc(graph, model, relation.to, relation.from, relation.id);
        }
    }
    for (auto &edges : graph.adjacency) {
        std::sort(edges.begin(), edges.end(), [&](const Edge &left, const Edge &right) {
            const auto left_id = model.external_id(object_from_ordinal(model, left.neighbor));
            const auto right_id = model.external_id(object_from_ordinal(model, right.neighbor));
            return left_id != right_id ? left_id < right_id : left.relation < right.relation;
        });
    }
    graph.status = AnalysisStatus::complete;
    return graph;
}

[[nodiscard]] Reachability run_reachability(const ViewGraph &graph, std::size_t source,
                                            std::size_t target, const Exclusions &excluded,
                                            const ServiceCutBudget &budget,
                                            ServiceCutUsage &usage) {
    Reachability result;
    if (source >= graph.adjacency.size() || target >= graph.adjacency.size() ||
        excluded.objects.size() != graph.adjacency.size() ||
        excluded.relations.size() != graph.selected_relations.size()) {
        return result;
    }
    if (excluded.objects[source] || excluded.objects[target] || !graph.selected_objects[source] ||
        !graph.selected_objects[target]) {
        result.status = AnalysisStatus::complete;
        return result;
    }
    if (usage.visited_objects >= budget.max_total_visited_objects) {
        result.status = AnalysisStatus::incomplete;
        return result;
    }
    std::vector<bool> reached(graph.adjacency.size(), false);
    std::deque<std::size_t> ready;
    reached[source] = true;
    ready.push_back(source);
    ++usage.visited_objects;
    while (!ready.empty()) {
        const auto current = ready.front();
        ready.pop_front();
        for (const auto &edge : graph.adjacency[current]) {
            if (usage.examined_relations >= budget.max_total_examined_relations) {
                result.status = AnalysisStatus::incomplete;
                return result;
            }
            ++usage.examined_relations;
            if (excluded.relations[edge.relation.value()] || excluded.objects[edge.neighbor] ||
                reached[edge.neighbor]) {
                continue;
            }
            if (usage.visited_objects >= budget.max_total_visited_objects) {
                result.status = AnalysisStatus::incomplete;
                return result;
            }
            reached[edge.neighbor] = true;
            ++usage.visited_objects;
            if (edge.neighbor == target) {
                result.status = AnalysisStatus::complete;
                result.reachable = true;
                return result;
            }
            ready.push_back(edge.neighbor);
        }
    }
    result.status = AnalysisStatus::complete;
    result.reachable = source == target;
    return result;
}

void add_flow_edge(std::vector<std::vector<FlowEdge>> &network, std::size_t from, std::size_t to,
                   std::size_t capacity) {
    const auto forward_index = network[to].size();
    const auto reverse_index = network[from].size();
    network[from].push_back({to, forward_index, capacity});
    network[to].push_back({from, reverse_index, 0});
}

[[nodiscard]] std::optional<std::size_t>
bounded_max_flow(const ViewGraph &graph, std::size_t source, std::size_t target, bool node_cut,
                 std::size_t limit, const ServiceCutBudget &budget, ServiceCutUsage &usage) {
    const auto count = graph.adjacency.size();
    const auto infinity = limit + 1;
    std::vector<std::vector<FlowEdge>> network(
        node_cut ? count * 2 : count + graph.selected_relations.size() * 2);
    if (node_cut) {
        for (std::size_t object = 0; object < count; ++object) {
            if (!graph.selected_objects[object]) {
                continue;
            }
            add_flow_edge(network, object * 2, object * 2 + 1,
                          object == source || object == target ? infinity : 1);
        }
        for (std::size_t from = 0; from < count; ++from) {
            for (const auto &edge : graph.adjacency[from]) {
                add_flow_edge(network, from * 2 + 1, edge.neighbor * 2, infinity);
            }
        }
        source = source * 2 + 1;
        target *= 2;
    } else {
        for (std::size_t relation = 0; relation < graph.selected_relations.size(); ++relation) {
            if (graph.selected_relations[relation]) {
                add_flow_edge(network, count + relation * 2, count + relation * 2 + 1, 1);
            }
        }
        for (std::size_t from = 0; from < count; ++from) {
            for (const auto &edge : graph.adjacency[from]) {
                const auto relation_in = count + edge.relation.value() * 2;
                add_flow_edge(network, from, relation_in, infinity);
                add_flow_edge(network, relation_in + 1, edge.neighbor, infinity);
            }
        }
    }
    std::size_t flow = 0;
    while (flow <= limit) {
        std::vector<std::pair<std::size_t, std::size_t>> predecessor(network.size(),
                                                                     {network.size(), 0});
        std::deque<std::size_t> ready;
        ready.push_back(source);
        predecessor[source] = {source, 0};
        while (!ready.empty() && predecessor[target].first == network.size()) {
            const auto current = ready.front();
            ready.pop_front();
            for (std::size_t index = 0; index < network[current].size(); ++index) {
                if (usage.flow_transitions >= budget.max_flow_transitions) {
                    return std::nullopt;
                }
                ++usage.flow_transitions;
                const auto &edge = network[current][index];
                if (edge.capacity == 0 || predecessor[edge.to].first != network.size()) {
                    continue;
                }
                predecessor[edge.to] = {current, index};
                ready.push_back(edge.to);
            }
        }
        if (predecessor[target].first == network.size()) {
            return flow;
        }
        for (auto current = target; current != source;) {
            const auto [previous, index] = predecessor[current];
            auto &edge = network[previous][index];
            --edge.capacity;
            ++network[current][edge.reverse].capacity;
            current = previous;
        }
        ++flow;
    }
    return flow;
}

[[nodiscard]] std::string_view element_id(const NormalizedModel &model,
                                          const CutElement &element) noexcept {
    switch (element.kind) {
    case CutElementKind::object:
        return model.external_id(element.object_value);
    case CutElementKind::relation:
        return model.string(model.relations[element.relation_value.value()].external_id);
    case CutElementKind::failure_domain:
        return model.string(
            model.failure_domains[element.failure_domain_value.value()].external_id);
    }
    return {};
}

[[nodiscard]] bool same_element(const CutElement &left, const CutElement &right) noexcept {
    return left.kind == right.kind &&
           ((left.kind == CutElementKind::object && left.object_value == right.object_value) ||
            (left.kind == CutElementKind::relation &&
             left.relation_value == right.relation_value) ||
            (left.kind == CutElementKind::failure_domain &&
             left.failure_domain_value == right.failure_domain_value));
}

void sort_elements(const NormalizedModel &model, std::vector<CutElement> &elements) {
    std::sort(
        elements.begin(), elements.end(), [&](const CutElement &left, const CutElement &right) {
            return left.kind != right.kind ? left.kind < right.kind
                                           : element_id(model, left) < element_id(model, right);
        });
    elements.erase(std::unique(elements.begin(), elements.end(), same_element), elements.end());
}

[[nodiscard]] bool valid_element(const NormalizedModel &model, const ViewGraph &graph,
                                 const CutElement &element, std::size_t source,
                                 std::size_t target) {
    switch (element.kind) {
    case CutElementKind::object:
        return valid_object(model, element.object_value) &&
               graph.selected_objects[ordinal(model, element.object_value)] &&
               ordinal(model, element.object_value) != source &&
               ordinal(model, element.object_value) != target;
    case CutElementKind::relation:
        return element.relation_value.is_valid() &&
               element.relation_value.value() < model.relations.size() &&
               graph.selected_relations[element.relation_value.value()];
    case CutElementKind::failure_domain:
        if (!element.failure_domain_value.is_valid() ||
            element.failure_domain_value.value() >= model.failure_domains.size()) {
            return false;
        }
        if (const auto expansion = expand_failure_domain(model, element.failure_domain_value)) {
            return std::none_of(expansion->members.begin(), expansion->members.end(),
                                [&](ObjectRef object) {
                                    const auto value = ordinal(model, object);
                                    return value == source || value == target;
                                });
        }
        return false;
    }
    return false;
}

[[nodiscard]] std::optional<Exclusions> make_exclusions(const NormalizedModel &model,
                                                        const std::vector<CutElement> &elements,
                                                        ServiceCutFinding &finding) {
    Exclusions excluded{std::vector<bool>(object_count(model), false),
                        std::vector<bool>(model.relations.size(), false)};
    for (const auto &element : elements) {
        switch (element.kind) {
        case CutElementKind::object:
            excluded.objects[ordinal(model, element.object_value)] = true;
            finding.removed_objects.push_back(element.object_value);
            break;
        case CutElementKind::relation:
            excluded.relations[element.relation_value.value()] = true;
            finding.removed_relations.push_back(element.relation_value);
            break;
        case CutElementKind::failure_domain: {
            const auto expansion = expand_failure_domain(model, element.failure_domain_value);
            if (!expansion.has_value()) {
                return std::nullopt;
            }
            finding.common_domains.push_back(element.failure_domain_value);
            for (const auto object : expansion->members) {
                excluded.objects[ordinal(model, object)] = true;
                finding.removed_objects.push_back(object);
            }
            break;
        }
        }
    }
    auto object_less = [&](ObjectRef left, ObjectRef right) {
        return model.external_id(left) < model.external_id(right);
    };
    std::sort(finding.removed_objects.begin(), finding.removed_objects.end(), object_less);
    finding.removed_objects.erase(
        std::unique(finding.removed_objects.begin(), finding.removed_objects.end()),
        finding.removed_objects.end());
    std::sort(finding.removed_relations.begin(), finding.removed_relations.end());
    finding.removed_relations.erase(
        std::unique(finding.removed_relations.begin(), finding.removed_relations.end()),
        finding.removed_relations.end());
    std::sort(finding.common_domains.begin(), finding.common_domains.end());
    return excluded;
}

[[nodiscard]] std::vector<CutElement> automatic_candidates(const NormalizedModel &model,
                                                           const ViewGraph &graph,
                                                           std::size_t source, std::size_t target) {
    std::vector<CutElement> result;
    for (std::size_t object = 0; object < graph.selected_objects.size(); ++object) {
        if (graph.selected_objects[object] && object != source && object != target) {
            result.push_back(CutElement::for_object(object_from_ordinal(model, object)));
        }
    }
    for (const auto &relation : model.relations) {
        if (graph.selected_relations[relation.id.value()]) {
            result.push_back(CutElement::for_relation(relation.id));
        }
    }
    for (const auto &domain : model.failure_domains) {
        const auto expansion = expand_failure_domain(model, domain.id);
        if (expansion.has_value() &&
            std::none_of(expansion->members.begin(), expansion->members.end(),
                         [&](ObjectRef object) {
                             const auto value = ordinal(model, object);
                             return value == source || value == target;
                         }) &&
            std::any_of(
                expansion->members.begin(), expansion->members.end(),
                [&](ObjectRef object) { return graph.selected_objects[ordinal(model, object)]; })) {
            result.push_back(CutElement::for_failure_domain(domain.id));
        }
    }
    sort_elements(model, result);
    return result;
}

} // namespace

ServiceCutAnalysisResult analyze_service_cuts(const NormalizedModel &model,
                                              const AvailabilitySnapshot &snapshot,
                                              const ServiceCutRequest &request,
                                              const ServiceCutBudget &budget) {
    ServiceCutAnalysisResult result;
    result.request = request;
    const auto graph = build_view(model, snapshot, request.view);
    if (graph.status != AnalysisStatus::complete || !valid_object(model, request.source) ||
        !valid_object(model, request.target) || request.source == request.target ||
        request.max_cut_size == 0 || request.max_cut_size > 2) {
        return result;
    }
    const auto source = ordinal(model, request.source);
    const auto target = ordinal(model, request.target);
    const Exclusions none{std::vector<bool>(object_count(model), false),
                          std::vector<bool>(model.relations.size(), false)};
    const auto baseline = run_reachability(graph, source, target, none, budget, result.usage);
    if (baseline.status != AnalysisStatus::complete) {
        result.status = baseline.status;
        return result;
    }
    result.baseline_reachable = baseline.reachable;
    if (!baseline.reachable) {
        result.status = AnalysisStatus::complete;
        return result;
    }
    const auto object_flow =
        bounded_max_flow(graph, source, target, true, request.max_cut_size, budget, result.usage);
    const auto relation_flow =
        bounded_max_flow(graph, source, target, false, request.max_cut_size, budget, result.usage);
    if (!object_flow.has_value() || !relation_flow.has_value()) {
        result.status = AnalysisStatus::incomplete;
        return result;
    }
    result.minimum_object_cut = *object_flow <= request.max_cut_size ? *object_flow : 0;
    result.minimum_relation_cut = *relation_flow <= request.max_cut_size ? *relation_flow : 0;

    auto candidates = request.candidates.empty()
                          ? automatic_candidates(model, graph, source, target)
                          : request.candidates;
    for (const auto &candidate : candidates) {
        if (!valid_element(model, graph, candidate, source, target)) {
            return result;
        }
    }
    sort_elements(model, candidates);
    result.request.candidates = candidates;
    result.status = AnalysisStatus::complete;
    std::vector<bool> single_is_cut(candidates.size(), false);
    const auto consider = [&](std::vector<CutElement> elements, ServiceCutAnalysisResult &output) {
        if (output.usage.candidate_sets_considered >= budget.max_candidate_sets ||
            output.findings.size() >= budget.max_results) {
            output.status = AnalysisStatus::incomplete;
            return false;
        }
        ++output.usage.candidate_sets_considered;
        ServiceCutFinding finding;
        finding.elements = std::move(elements);
        finding.qualification = graph.qualification;
        finding.contains_unknown_availability = graph.contains_unknown;
        const auto exclusions = make_exclusions(model, finding.elements, finding);
        if (!exclusions.has_value()) {
            output.status = AnalysisStatus::invalid;
            return false;
        }
        const auto changed =
            run_reachability(graph, source, target, *exclusions, budget, output.usage);
        if (changed.status != AnalysisStatus::complete) {
            output.status = changed.status;
            return false;
        }
        if (!changed.reachable) {
            output.findings.push_back(std::move(finding));
            return true;
        }
        return false;
    };
    for (std::size_t index = 0; index < candidates.size(); ++index) {
        single_is_cut[index] = consider({candidates[index]}, result);
        if (result.status != AnalysisStatus::complete) {
            return result;
        }
    }
    if (request.max_cut_size == 2) {
        for (std::size_t left = 0; left < candidates.size(); ++left) {
            if (single_is_cut[left] || candidates[left].kind == CutElementKind::failure_domain) {
                continue;
            }
            for (std::size_t right = left + 1; right < candidates.size(); ++right) {
                if (single_is_cut[right] ||
                    candidates[right].kind == CutElementKind::failure_domain ||
                    (request.candidates.empty() &&
                     candidates[left].kind != candidates[right].kind)) {
                    continue;
                }
                (void)consider({candidates[left], candidates[right]}, result);
                if (result.status != AnalysisStatus::complete) {
                    return result;
                }
            }
        }
    }
    return result;
}

} // namespace ltfa
