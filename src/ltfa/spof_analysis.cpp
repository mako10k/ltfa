#include "ltfa/spof_analysis.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <optional>
#include <string_view>
#include <utility>

namespace ltfa {
namespace {

struct ViewEdge {
    std::size_t neighbor{0};
    RelationId relation;
};

struct ViewGraph {
    AnalysisStatus status{AnalysisStatus::invalid};
    std::vector<bool> selected_objects;
    std::vector<bool> selected_relations;
    std::vector<std::vector<ViewEdge>> adjacency;
    AssuranceQualification qualification;
    bool contains_unknown_availability{false};
    bool alternatives_excluded_by_filter{false};
    TraversalUsage usage;
};

struct BfsResult {
    AnalysisStatus status{AnalysisStatus::invalid};
    std::vector<bool> reached;
    std::vector<ObjectRef> predecessor;
    std::vector<RelationId> predecessor_relation;
};

struct TarjanFrame {
    std::size_t object{0};
    std::size_t next_edge{0};
};

[[nodiscard]] std::size_t object_count(const NormalizedModel &model) noexcept {
    return model.entities.size() + model.endpoints.size();
}

[[nodiscard]] bool valid_object(const NormalizedModel &model, ObjectRef object) noexcept {
    if (!object.is_valid()) {
        return false;
    }
    switch (object.kind) {
    case ObjectKind::entity:
        return object.index < model.entities.size();
    case ObjectKind::endpoint:
        return object.index < model.endpoints.size();
    }
    return false;
}

[[nodiscard]] std::size_t object_ordinal(const NormalizedModel &model, ObjectRef object) noexcept {
    return object.kind == ObjectKind::entity ? object.index : model.entities.size() + object.index;
}

[[nodiscard]] ObjectRef object_from_ordinal(const NormalizedModel &model,
                                            std::size_t ordinal) noexcept {
    if (ordinal < model.entities.size()) {
        return ObjectRef::from(EntityId{static_cast<std::uint32_t>(ordinal)});
    }
    return ObjectRef::from(EndpointId{static_cast<std::uint32_t>(ordinal - model.entities.size())});
}

[[nodiscard]] bool valid_range(IndexRange range, std::size_t size) noexcept {
    return range.begin <= size && range.count <= size - range.begin;
}

[[nodiscard]] bool valid_availability_state(AvailabilityState state) noexcept {
    switch (state) {
    case AvailabilityState::available:
    case AvailabilityState::degraded:
    case AvailabilityState::unavailable:
    case AvailabilityState::unknown:
        return true;
    }
    return false;
}

[[nodiscard]] std::optional<EntityId> owner_entity(const NormalizedModel &model,
                                                   ObjectRef object) noexcept {
    if (!valid_object(model, object)) {
        return std::nullopt;
    }
    const auto owner = object.kind == ObjectKind::entity ? EntityId{object.index}
                                                         : model.endpoints[object.index].owner;
    if (!owner.is_valid() || owner.value() >= model.entities.size()) {
        return std::nullopt;
    }
    return owner;
}

[[nodiscard]] std::optional<ScopeId> object_scope(const NormalizedModel &model,
                                                  ObjectRef object) noexcept {
    if (!valid_object(model, object)) {
        return std::nullopt;
    }
    const auto scope = object.kind == ObjectKind::entity ? model.entities[object.index].scope
                                                         : model.endpoints[object.index].scope;
    if (!scope.is_valid() || scope.value() >= model.scopes.size()) {
        return std::nullopt;
    }
    return scope;
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

void sort_objects(const NormalizedModel &model, std::vector<ObjectRef> &objects) {
    std::sort(objects.begin(), objects.end(), [&](ObjectRef left, ObjectRef right) {
        const auto left_id = model.external_id(left);
        const auto right_id = model.external_id(right);
        if (left_id != right_id) {
            return left_id < right_id;
        }
        if (left.kind != right.kind) {
            return left.kind < right.kind;
        }
        return left.index < right.index;
    });
    objects.erase(std::unique(objects.begin(), objects.end()), objects.end());
}

void sort_relations(const NormalizedModel &model, std::vector<RelationId> &relations) {
    std::sort(relations.begin(), relations.end(), [&](RelationId left, RelationId right) {
        return model.string(model.relations[left.value()].external_id) <
               model.string(model.relations[right.value()].external_id);
    });
    relations.erase(std::unique(relations.begin(), relations.end()), relations.end());
}

void sort_domains(const NormalizedModel &model, std::vector<FailureDomainId> &domains) {
    std::sort(domains.begin(), domains.end(), [&](FailureDomainId left, FailureDomainId right) {
        return model.string(model.failure_domains[left.value()].external_id) <
               model.string(model.failure_domains[right.value()].external_id);
    });
    domains.erase(std::unique(domains.begin(), domains.end()), domains.end());
}

[[nodiscard]] bool relation_kind_selected(const GraphFilter &filter, RelationKind kind) noexcept {
    return filter.relation_kinds.empty() ||
           std::find(filter.relation_kinds.begin(), filter.relation_kinds.end(), kind) !=
               filter.relation_kinds.end();
}

[[nodiscard]] bool valid_filter(const NormalizedModel &model, const GraphFilter &filter) noexcept {
    if (filter.plane.has_value() &&
        static_cast<std::size_t>(*filter.plane) > static_cast<std::size_t>(Plane::service)) {
        return false;
    }
    if (filter.layer.has_value() &&
        static_cast<std::size_t>(*filter.layer) > static_cast<std::size_t>(Layer::application)) {
        return false;
    }
    if (filter.scope.has_value() &&
        (!filter.scope->is_valid() || filter.scope->value() >= model.scopes.size())) {
        return false;
    }
    for (const auto kind : filter.relation_kinds) {
        if (static_cast<std::size_t>(kind) >= kRelationKindCount) {
            return false;
        }
    }
    switch (filter.direction) {
    case TraversalDirection::forward:
    case TraversalDirection::reverse:
    case TraversalDirection::undirected:
        return true;
    }
    return false;
}

[[nodiscard]] std::optional<std::vector<bool>> failure_domain_mask(const NormalizedModel &model,
                                                                   const GraphFilter &filter) {
    if (!filter.failure_domain.has_value()) {
        return std::vector<bool>{};
    }
    const auto expansion = expand_failure_domain(model, *filter.failure_domain);
    if (!expansion.has_value()) {
        return std::nullopt;
    }
    std::vector<bool> selected(object_count(model), false);
    for (const auto member : expansion->members) {
        selected[object_ordinal(model, member)] = true;
    }
    return selected;
}

[[nodiscard]] std::optional<bool>
object_matches_filter(const NormalizedModel &model, const AvailabilitySnapshot &snapshot,
                      ObjectRef object, const GraphFilter &filter,
                      const std::vector<bool> &domain_mask) noexcept {
    const auto owner = owner_entity(model, object);
    const auto scope = object_scope(model, object);
    if (!owner.has_value() || !scope.has_value()) {
        return std::nullopt;
    }
    const auto state = snapshot.state(object);
    if (!valid_availability_state(state)) {
        return std::nullopt;
    }
    const auto &entity = model.entities[owner->value()];
    return (!filter.plane.has_value() || entity.plane == *filter.plane) &&
           (!filter.layer.has_value() || entity.layer == *filter.layer) &&
           (!filter.scope.has_value() || *scope == *filter.scope) &&
           (!filter.failure_domain.has_value() || domain_mask[object_ordinal(model, object)]) &&
           filter.availability.includes(state);
}

[[nodiscard]] ViewGraph build_view(const NormalizedModel &model,
                                   const AvailabilitySnapshot &snapshot, const GraphFilter &filter,
                                   const TraversalBudget &budget) {
    ViewGraph result;
    result.selected_objects.resize(object_count(model), false);
    result.selected_relations.resize(model.relations.size(), false);
    result.adjacency.resize(object_count(model));
    if (snapshot.entity_states.size() != model.entities.size() ||
        snapshot.endpoint_states.size() != model.endpoints.size() || !valid_filter(model, filter)) {
        return result;
    }
    const auto domain_mask = failure_domain_mask(model, filter);
    if (!domain_mask.has_value()) {
        return result;
    }

    result.qualification = {Completeness::verified, EvidenceQuality::confirmed};
    bool has_selected_object = false;
    for (std::size_t ordinal = 0; ordinal < object_count(model); ++ordinal) {
        const auto object = object_from_ordinal(model, ordinal);
        const auto selected = object_matches_filter(model, snapshot, object, filter, *domain_mask);
        if (!selected.has_value()) {
            return result;
        }
        if (!*selected) {
            continue;
        }
        if (result.usage.visited_objects >= budget.max_visited_objects) {
            result.status = AnalysisStatus::incomplete;
            return result;
        }
        const auto scope = object_scope(model, object);
        const auto &scope_value = model.scopes[scope->value()];
        result.selected_objects[ordinal] = true;
        ++result.usage.visited_objects;
        has_selected_object = true;
        combine_qualification(result.qualification,
                              {scope_value.completeness, scope_value.evidence_quality});
        if (snapshot.state(object) == AvailabilityState::unknown) {
            result.contains_unknown_availability = true;
        }
    }
    if (!has_selected_object) {
        result.qualification = {Completeness::unknown, EvidenceQuality::unknown};
    }

    for (std::size_t ordinal = 0; ordinal < model.relations.size(); ++ordinal) {
        const auto &relation = model.relations[ordinal];
        if (result.usage.examined_relations >= budget.max_examined_relations) {
            result.status = AnalysisStatus::incomplete;
            return result;
        }
        ++result.usage.examined_relations;
        if (!relation.id.is_valid() || relation.id.value() >= model.relations.size() ||
            relation.id.value() != ordinal || !valid_object(model, relation.from) ||
            !valid_object(model, relation.to) ||
            static_cast<std::size_t>(relation.kind) >= kRelationKindCount) {
            return result;
        }
        switch (relation.direction) {
        case Direction::directed:
        case Direction::bidirectional:
            break;
        default:
            return result;
        }
        const bool from_selected = result.selected_objects[object_ordinal(model, relation.from)];
        const bool to_selected = result.selected_objects[object_ordinal(model, relation.to)];
        const bool kind_selected = relation_kind_selected(filter, relation.kind);
        if (from_selected && to_selected && kind_selected) {
            result.selected_relations[relation.id.value()] = true;
            const auto from = object_ordinal(model, relation.from);
            const auto to = object_ordinal(model, relation.to);
            result.adjacency[from].push_back({to, relation.id});
            result.adjacency[to].push_back({from, relation.id});
        } else if ((from_selected && to_selected && !kind_selected) ||
                   (kind_selected && from_selected != to_selected)) {
            result.alternatives_excluded_by_filter = true;
        }
    }
    result.status = AnalysisStatus::complete;
    return result;
}

void run_tarjan(const NormalizedModel &model, const ViewGraph &graph,
                std::vector<ObjectRef> &articulation_points, std::vector<RelationId> &bridges) {
    const auto unvisited = std::numeric_limits<std::size_t>::max();
    std::vector<std::size_t> discovery(object_count(model), unvisited);
    std::vector<std::size_t> low(object_count(model), unvisited);
    std::vector<std::size_t> parent(object_count(model), unvisited);
    std::vector<RelationId> parent_relation(object_count(model));
    std::vector<std::size_t> child_count(object_count(model), 0);
    std::vector<bool> articulation(object_count(model), false);
    std::vector<TarjanFrame> stack;
    std::size_t next_discovery = 0;

    for (std::size_t root = 0; root < object_count(model); ++root) {
        if (!graph.selected_objects[root] || discovery[root] != unvisited) {
            continue;
        }
        discovery[root] = next_discovery;
        low[root] = next_discovery;
        ++next_discovery;
        stack.push_back({root, 0});

        while (!stack.empty()) {
            auto &frame = stack.back();
            if (frame.next_edge < graph.adjacency[frame.object].size()) {
                const auto edge = graph.adjacency[frame.object][frame.next_edge++];
                if (discovery[edge.neighbor] == unvisited) {
                    parent[edge.neighbor] = frame.object;
                    parent_relation[edge.neighbor] = edge.relation;
                    ++child_count[frame.object];
                    discovery[edge.neighbor] = next_discovery;
                    low[edge.neighbor] = next_discovery;
                    ++next_discovery;
                    stack.push_back({edge.neighbor, 0});
                } else if (edge.relation != parent_relation[frame.object]) {
                    low[frame.object] = std::min(low[frame.object], discovery[edge.neighbor]);
                }
                continue;
            }

            const auto object = frame.object;
            stack.pop_back();
            if (parent[object] == unvisited) {
                articulation[object] = child_count[object] > 1;
                continue;
            }
            const auto parent_object = parent[object];
            low[parent_object] = std::min(low[parent_object], low[object]);
            if (low[object] > discovery[parent_object]) {
                bridges.push_back(parent_relation[object]);
            }
            if (parent[parent_object] != unvisited && low[object] >= discovery[parent_object]) {
                articulation[parent_object] = true;
            }
        }
    }

    for (std::size_t ordinal = 0; ordinal < articulation.size(); ++ordinal) {
        if (articulation[ordinal]) {
            articulation_points.push_back(object_from_ordinal(model, ordinal));
        }
    }
    sort_objects(model, articulation_points);
    sort_relations(model, bridges);
}

[[nodiscard]] bool focus_is_valid(const NormalizedModel &model, const ViewGraph &graph,
                                  const std::vector<ObjectRef> &focus) noexcept {
    return std::all_of(focus.begin(), focus.end(), [&](ObjectRef object) {
        return valid_object(model, object) && graph.selected_objects[object_ordinal(model, object)];
    });
}

[[nodiscard]] StructuralAnalysisResult
make_structural_result(const NormalizedModel &model, const ViewGraph &graph, GraphFilter filter,
                       std::vector<ObjectRef> sources, std::vector<ObjectRef> targets) {
    StructuralAnalysisResult result;
    result.status = graph.status;
    filter.direction = TraversalDirection::undirected;
    result.view = std::move(filter);
    result.sources = std::move(sources);
    result.targets = std::move(targets);
    result.qualification = graph.qualification;
    result.contains_unknown_availability = graph.contains_unknown_availability;
    result.alternatives_excluded_by_filter = graph.alternatives_excluded_by_filter;
    result.usage = graph.usage;
    if (result.status != AnalysisStatus::complete ||
        !focus_is_valid(model, graph, result.sources) ||
        !focus_is_valid(model, graph, result.targets)) {
        if (result.status == AnalysisStatus::complete) {
            result.status = AnalysisStatus::invalid;
        }
        return result;
    }
    sort_objects(model, result.sources);
    sort_objects(model, result.targets);
    run_tarjan(model, graph, result.articulation_points, result.bridges);
    return result;
}

[[nodiscard]] BfsResult run_bfs(const NormalizedModel &model, const ViewGraph &graph,
                                ObjectRef source, const std::vector<bool> &excluded_objects,
                                const std::vector<bool> &excluded_relations,
                                const SpofBudget &budget, SpofUsage &usage) {
    BfsResult result;
    result.reached.resize(object_count(model), false);
    result.predecessor.resize(object_count(model));
    result.predecessor_relation.resize(object_count(model));
    if (!valid_object(model, source) || excluded_objects.size() != object_count(model) ||
        excluded_relations.size() != model.relations.size()) {
        return result;
    }
    const auto source_ordinal = object_ordinal(model, source);
    if (!graph.selected_objects[source_ordinal] || excluded_objects[source_ordinal]) {
        result.status = AnalysisStatus::complete;
        return result;
    }
    if (usage.visited_objects >= budget.max_total_visited_objects) {
        result.status = AnalysisStatus::incomplete;
        return result;
    }

    std::deque<std::size_t> ready;
    result.reached[source_ordinal] = true;
    ++usage.visited_objects;
    ready.push_back(source_ordinal);
    while (!ready.empty()) {
        const auto current = ready.front();
        ready.pop_front();
        for (const auto &edge : graph.adjacency[current]) {
            if (usage.examined_relations >= budget.max_total_examined_relations) {
                result.status = AnalysisStatus::incomplete;
                return result;
            }
            ++usage.examined_relations;
            if (excluded_relations[edge.relation.value()] || excluded_objects[edge.neighbor] ||
                result.reached[edge.neighbor]) {
                continue;
            }
            if (usage.visited_objects >= budget.max_total_visited_objects) {
                result.status = AnalysisStatus::incomplete;
                return result;
            }
            result.reached[edge.neighbor] = true;
            result.predecessor[edge.neighbor] = object_from_ordinal(model, current);
            result.predecessor_relation[edge.neighbor] = edge.relation;
            ++usage.visited_objects;
            ready.push_back(edge.neighbor);
        }
    }
    result.status = AnalysisStatus::complete;
    return result;
}

[[nodiscard]] std::optional<SpofPathEvidence> path_evidence(const NormalizedModel &model,
                                                            ObjectRef source, ObjectRef target,
                                                            const BfsResult &bfs) {
    if (!valid_object(model, target) || bfs.reached.size() != object_count(model) ||
        !bfs.reached[object_ordinal(model, target)]) {
        return std::nullopt;
    }
    SpofPathEvidence result;
    result.target = target;
    auto current = target;
    while (current != source) {
        result.objects.push_back(current);
        const auto ordinal = object_ordinal(model, current);
        const auto predecessor = bfs.predecessor[ordinal];
        const auto relation = bfs.predecessor_relation[ordinal];
        if (!valid_object(model, predecessor) || !relation.is_valid() ||
            relation.value() >= model.relations.size()) {
            return std::nullopt;
        }
        result.relations.push_back(relation);
        current = predecessor;
    }
    result.objects.push_back(source);
    std::reverse(result.objects.begin(), result.objects.end());
    std::reverse(result.relations.begin(), result.relations.end());
    return result;
}

[[nodiscard]] bool valid_removal_target(const NormalizedModel &model, const ViewGraph &graph,
                                        RemovalTarget candidate) {
    switch (candidate.kind) {
    case RemovalKind::object:
        return valid_object(model, candidate.object_value) &&
               graph.selected_objects[object_ordinal(model, candidate.object_value)];
    case RemovalKind::relation:
        return candidate.relation_value.is_valid() &&
               candidate.relation_value.value() < model.relations.size() &&
               model.relations[candidate.relation_value.value()].id == candidate.relation_value &&
               graph.selected_relations[candidate.relation_value.value()];
    case RemovalKind::failure_domain: {
        const auto expansion = expand_failure_domain(model, candidate.failure_domain_value);
        return expansion.has_value() &&
               std::any_of(expansion->members.begin(), expansion->members.end(),
                           [&](ObjectRef member) {
                               return graph.selected_objects[object_ordinal(model, member)];
                           });
    }
    }
    return false;
}

[[nodiscard]] std::string_view candidate_external_id(const NormalizedModel &model,
                                                     RemovalTarget candidate) {
    switch (candidate.kind) {
    case RemovalKind::object:
        return model.external_id(candidate.object_value);
    case RemovalKind::relation:
        return model.string(model.relations[candidate.relation_value.value()].external_id);
    case RemovalKind::failure_domain:
        return model.string(
            model.failure_domains[candidate.failure_domain_value.value()].external_id);
    }
    return {};
}

[[nodiscard]] bool same_candidate(RemovalTarget left, RemovalTarget right) noexcept {
    if (left.kind != right.kind) {
        return false;
    }
    switch (left.kind) {
    case RemovalKind::object:
        return left.object_value == right.object_value;
    case RemovalKind::relation:
        return left.relation_value == right.relation_value;
    case RemovalKind::failure_domain:
        return left.failure_domain_value == right.failure_domain_value;
    }
    return false;
}

void sort_candidates(const NormalizedModel &model, std::vector<RemovalTarget> &candidates) {
    std::sort(candidates.begin(), candidates.end(), [&](RemovalTarget left, RemovalTarget right) {
        if (left.kind != right.kind) {
            return left.kind < right.kind;
        }
        return candidate_external_id(model, left) < candidate_external_id(model, right);
    });
    candidates.erase(std::unique(candidates.begin(), candidates.end(), same_candidate),
                     candidates.end());
}

[[nodiscard]] std::optional<std::pair<std::vector<bool>, std::vector<bool>>>
candidate_exclusions(const NormalizedModel &model, const ViewGraph &graph, RemovalTarget candidate,
                     std::vector<ObjectRef> &common_cause_members) {
    std::vector<bool> objects(object_count(model), false);
    std::vector<bool> relations(model.relations.size(), false);
    switch (candidate.kind) {
    case RemovalKind::object:
        objects[object_ordinal(model, candidate.object_value)] = true;
        break;
    case RemovalKind::relation:
        relations[candidate.relation_value.value()] = true;
        break;
    case RemovalKind::failure_domain: {
        const auto expansion = expand_failure_domain(model, candidate.failure_domain_value);
        if (!expansion.has_value()) {
            return std::nullopt;
        }
        for (const auto member : expansion->members) {
            if (!graph.selected_objects[object_ordinal(model, member)]) {
                continue;
            }
            objects[object_ordinal(model, member)] = true;
            common_cause_members.push_back(member);
        }
        sort_objects(model, common_cause_members);
        break;
    }
    }
    return std::pair{std::move(objects), std::move(relations)};
}

[[nodiscard]] bool append_object_domains(const NormalizedModel &model, ObjectRef object,
                                         std::vector<FailureDomainId> &domains) {
    const auto range = object.kind == ObjectKind::entity
                           ? model.entities[object.index].failure_domains
                           : model.endpoints[object.index].failure_domains;
    if (!valid_range(range, model.object_failure_domains.size())) {
        return false;
    }
    for (std::uint32_t offset = 0; offset < range.count; ++offset) {
        const auto domain = model.object_failure_domains[range.begin + offset];
        if (!domain.is_valid() || domain.value() >= model.failure_domains.size()) {
            return false;
        }
        domains.push_back(domain);
    }
    sort_domains(model, domains);
    return true;
}

[[nodiscard]] std::optional<std::vector<RemovalTarget>>
automatic_candidates(const NormalizedModel &model, const ViewGraph &graph,
                     const StructuralAnalysisResult &structural, ObjectRef source,
                     const std::vector<ObjectRef> &targets) {
    std::vector<RemovalTarget> result;
    for (const auto object : structural.articulation_points) {
        if (object != source &&
            std::find(targets.begin(), targets.end(), object) == targets.end()) {
            result.push_back(RemovalTarget::for_object(object));
        }
    }
    for (const auto relation : structural.bridges) {
        result.push_back(RemovalTarget::for_relation(relation));
    }
    for (const auto &domain : model.failure_domains) {
        const auto expansion = expand_failure_domain(model, domain.id);
        if (!expansion.has_value()) {
            return std::nullopt;
        }
        const auto selected_members = std::count_if(
            expansion->members.begin(), expansion->members.end(), [&](ObjectRef member) {
                return graph.selected_objects[object_ordinal(model, member)];
            });
        if (selected_members >= 2) {
            result.push_back(RemovalTarget::for_failure_domain(domain.id));
        }
    }
    sort_candidates(model, result);
    return result;
}

[[nodiscard]] SpofClassification classify_spof(const ViewGraph &graph,
                                               bool target_is_affected) noexcept {
    if (!target_is_affected) {
        return SpofClassification::not_a_spof_in_view;
    }
    if (graph.qualification.completeness == Completeness::verified &&
        graph.qualification.evidence_quality == EvidenceQuality::confirmed &&
        !graph.contains_unknown_availability && !graph.alternatives_excluded_by_filter) {
        return SpofClassification::confirmed_spof;
    }
    return SpofClassification::model_spof;
}

void append_remaining_unknowns(const NormalizedModel &model, const AvailabilitySnapshot &snapshot,
                               const std::vector<SpofPathEvidence> &baseline_paths,
                               const std::vector<SpofPathEvidence> &retained_paths,
                               const std::vector<ObjectRef> &affected,
                               const std::vector<ObjectRef> &common_cause_members,
                               std::vector<ObjectRef> &unknowns) {
    for (const auto &path : baseline_paths) {
        unknowns.insert(unknowns.end(), path.objects.begin(), path.objects.end());
    }
    for (const auto &path : retained_paths) {
        unknowns.insert(unknowns.end(), path.objects.begin(), path.objects.end());
    }
    unknowns.insert(unknowns.end(), affected.begin(), affected.end());
    unknowns.insert(unknowns.end(), common_cause_members.begin(), common_cause_members.end());
    unknowns.erase(std::remove_if(unknowns.begin(), unknowns.end(),
                                  [&](ObjectRef object) {
                                      return snapshot.state(object) != AvailabilityState::unknown;
                                  }),
                   unknowns.end());
    sort_objects(model, unknowns);
}

} // namespace

std::string_view to_string(SpofClassification classification) noexcept {
    switch (classification) {
    case SpofClassification::confirmed_spof:
        return "confirmed-spof";
    case SpofClassification::model_spof:
        return "model-spof";
    case SpofClassification::not_a_spof_in_view:
        return "not-a-spof-in-view";
    }
    return "unknown";
}

StructuralAnalysisResult
analyze_articulation_bridges(const NormalizedModel &model, const AvailabilitySnapshot &snapshot,
                             const GraphFilter &view, const std::vector<ObjectRef> &sources,
                             const std::vector<ObjectRef> &targets, const TraversalBudget &budget) {
    const auto graph = build_view(model, snapshot, view, budget);
    return make_structural_result(model, graph, view, sources, targets);
}

SpofAnalysisResult analyze_spof(const NormalizedModel &model, const AvailabilitySnapshot &snapshot,
                                const SpofRequest &request, const SpofBudget &budget) {
    SpofAnalysisResult result;
    result.request = request;
    TraversalBudget structural_budget;
    structural_budget.max_visited_objects = budget.max_total_visited_objects;
    structural_budget.max_examined_relations = budget.max_total_examined_relations;
    const auto graph = build_view(model, snapshot, request.view, structural_budget);
    result.structural = make_structural_result(model, graph, result.request.view,
                                               {result.request.source}, result.request.targets);
    result.usage.visited_objects = graph.usage.visited_objects;
    result.usage.examined_relations = graph.usage.examined_relations;
    if (result.structural.status != AnalysisStatus::complete || result.request.targets.empty() ||
        !valid_object(model, result.request.source)) {
        result.status = result.structural.status == AnalysisStatus::incomplete
                            ? AnalysisStatus::incomplete
                            : AnalysisStatus::invalid;
        return result;
    }
    result.request.targets = result.structural.targets;
    for (const auto target : result.request.targets) {
        if (target == result.request.source) {
            return result;
        }
    }

    const std::vector<bool> no_objects(object_count(model), false);
    const std::vector<bool> no_relations(model.relations.size(), false);
    const auto baseline = run_bfs(model, graph, result.request.source, no_objects, no_relations,
                                  budget, result.usage);
    if (baseline.status != AnalysisStatus::complete) {
        result.status = baseline.status;
        return result;
    }
    std::vector<SpofPathEvidence> baseline_paths;
    for (const auto target : result.request.targets) {
        const auto path = path_evidence(model, result.request.source, target, baseline);
        if (!path.has_value()) {
            return result;
        }
        baseline_paths.push_back(*path);
    }

    const bool explicit_candidates = !result.request.candidates.empty();
    std::vector<RemovalTarget> candidates;
    if (explicit_candidates) {
        candidates = result.request.candidates;
    } else {
        const auto automatic = automatic_candidates(model, graph, result.structural,
                                                    result.request.source, result.request.targets);
        if (!automatic.has_value()) {
            return result;
        }
        candidates = *automatic;
    }
    for (const auto candidate : candidates) {
        if (!valid_removal_target(model, graph, candidate) ||
            (candidate.kind == RemovalKind::object &&
             (candidate.object_value == result.request.source ||
              std::find(result.request.targets.begin(), result.request.targets.end(),
                        candidate.object_value) != result.request.targets.end()))) {
            return result;
        }
    }
    sort_candidates(model, candidates);
    result.request.candidates = candidates;

    result.status = AnalysisStatus::complete;
    if (candidates.size() > budget.max_candidates) {
        candidates.resize(budget.max_candidates);
        result.status = AnalysisStatus::incomplete;
    }
    for (const auto candidate : candidates) {
        SpofFinding finding;
        finding.candidate = candidate;
        finding.baseline_paths = baseline_paths;
        finding.qualification = graph.qualification;
        finding.alternatives_excluded_by_filter = graph.alternatives_excluded_by_filter;
        const auto exclusions =
            candidate_exclusions(model, graph, candidate, finding.common_cause_members);
        if (!exclusions.has_value()) {
            result.status = AnalysisStatus::invalid;
            return result;
        }
        if (candidate.kind == RemovalKind::failure_domain) {
            finding.common_domains.push_back(candidate.failure_domain_value);
        } else if (candidate.kind == RemovalKind::object &&
                   !append_object_domains(model, candidate.object_value, finding.common_domains)) {
            result.status = AnalysisStatus::invalid;
            return result;
        }

        const auto changed = run_bfs(model, graph, result.request.source, exclusions->first,
                                     exclusions->second, budget, result.usage);
        if (changed.status != AnalysisStatus::complete) {
            result.status = changed.status;
            return result;
        }
        ++result.usage.candidates_considered;
        for (std::size_t ordinal = 0; ordinal < object_count(model); ++ordinal) {
            if (baseline.reached[ordinal] && !changed.reached[ordinal]) {
                finding.affected_objects.push_back(object_from_ordinal(model, ordinal));
            }
        }
        sort_objects(model, finding.affected_objects);
        for (const auto target : result.request.targets) {
            if (!changed.reached[object_ordinal(model, target)]) {
                finding.affected_targets.push_back(target);
                continue;
            }
            const auto retained = path_evidence(model, result.request.source, target, changed);
            if (!retained.has_value()) {
                result.status = AnalysisStatus::invalid;
                return result;
            }
            finding.retained_paths.push_back(*retained);
        }
        sort_objects(model, finding.affected_targets);
        finding.classification = classify_spof(graph, !finding.affected_targets.empty());
        append_remaining_unknowns(model, snapshot, finding.baseline_paths, finding.retained_paths,
                                  finding.affected_objects, finding.common_cause_members,
                                  finding.remaining_unknowns);
        if (explicit_candidates || !finding.affected_targets.empty()) {
            result.findings.push_back(std::move(finding));
        }
    }
    return result;
}

} // namespace ltfa
