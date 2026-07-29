#include "ltfa/path_analysis.hpp"

#include <algorithm>
#include <cstddef>
#include <deque>
#include <limits>

namespace ltfa {
namespace {

struct Exclusions {
    std::vector<bool> objects;
    std::vector<bool> relations;
};

struct TraversalState {
    AnalysisStatus status{AnalysisStatus::invalid};
    std::vector<bool> reached;
    std::vector<ObjectRef> predecessor;
    std::vector<RelationId> predecessor_relation;
    std::vector<ObjectRef> discovered;
    TraversalUsage usage;
};

struct QualificationResult {
    AssuranceQualification qualification;
    bool contains_unknown_availability{false};
};

[[nodiscard]] bool valid_object(const NormalizedModel &model, ObjectRef object) noexcept {
    if (!object.is_valid()) {
        return false;
    }
    return object.kind == ObjectKind::entity ? object.index < model.entities.size()
                                             : object.index < model.endpoints.size();
}

[[nodiscard]] std::size_t object_count(const NormalizedModel &model) noexcept {
    return model.entities.size() + model.endpoints.size();
}

[[nodiscard]] std::size_t object_ordinal(const NormalizedModel &model, ObjectRef object) noexcept {
    return object.kind == ObjectKind::entity ? object.index : model.entities.size() + object.index;
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

[[nodiscard]] bool valid_range(IndexRange range, std::size_t size) noexcept {
    return range.begin <= size && range.count <= size - range.begin;
}

[[nodiscard]] bool relation_kind_selected(const GraphFilter &filter, RelationKind kind) noexcept {
    return filter.relation_kinds.empty() ||
           std::find(filter.relation_kinds.begin(), filter.relation_kinds.end(), kind) !=
               filter.relation_kinds.end();
}

[[nodiscard]] bool valid_relation_kind(RelationKind kind) noexcept {
    return static_cast<std::size_t>(kind) < kRelationKindCount;
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

[[nodiscard]] bool snapshot_matches_model(const NormalizedModel &model,
                                          const AvailabilitySnapshot &snapshot) noexcept {
    return snapshot.entity_states.size() == model.entities.size() &&
           snapshot.endpoint_states.size() == model.endpoints.size();
}

[[nodiscard]] bool object_matches_filter(const NormalizedModel &model,
                                         const AvailabilitySnapshot &snapshot, ObjectRef object,
                                         const GraphFilter &filter,
                                         const std::vector<bool> &domain_mask) noexcept {
    const auto owner = owner_entity(model, object);
    const auto scope = object_scope(model, object);
    if (!owner.has_value() || !scope.has_value()) {
        return false;
    }
    const auto &entity = model.entities[owner->value()];
    if (filter.plane.has_value() && entity.plane != *filter.plane) {
        return false;
    }
    if (filter.layer.has_value() && entity.layer != *filter.layer) {
        return false;
    }
    if (filter.scope.has_value() && *scope != *filter.scope) {
        return false;
    }
    if (filter.failure_domain.has_value() && !domain_mask[object_ordinal(model, object)]) {
        return false;
    }
    return filter.availability.includes(snapshot.state(object));
}

[[nodiscard]] const ObjectRelationAdjacency *object_adjacency(const NormalizedModel &model,
                                                              ObjectRef object) noexcept {
    if (!valid_object(model, object)) {
        return nullptr;
    }
    if (object.kind == ObjectKind::entity) {
        if (object.index >= model.entity_relation_adjacency.size()) {
            return nullptr;
        }
        return &model.entity_relation_adjacency[object.index];
    }
    if (object.index >= model.endpoint_relation_adjacency.size()) {
        return nullptr;
    }
    return &model.endpoint_relation_adjacency[object.index];
}

[[nodiscard]] bool is_excluded(const NormalizedModel &model, ObjectRef object,
                               const Exclusions &exclusions) noexcept {
    return exclusions.objects[object_ordinal(model, object)];
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

[[nodiscard]] std::optional<QualificationResult>
qualify_objects(const NormalizedModel &model, const AvailabilitySnapshot &snapshot,
                const std::vector<ObjectRef> &objects) noexcept {
    QualificationResult result;
    result.qualification = {Completeness::verified, EvidenceQuality::confirmed};
    if (objects.empty()) {
        result.qualification = {Completeness::unknown, EvidenceQuality::unknown};
        return result;
    }
    for (const auto object : objects) {
        const auto scope = object_scope(model, object);
        if (!scope.has_value()) {
            return std::nullopt;
        }
        const auto &scope_value = model.scopes[scope->value()];
        combine_qualification(result.qualification,
                              {scope_value.completeness, scope_value.evidence_quality});
        if (snapshot.state(object) == AvailabilityState::unknown) {
            result.contains_unknown_availability = true;
        }
    }
    return result;
}

[[nodiscard]] bool valid_filter(const NormalizedModel &model, const GraphFilter &filter) noexcept {
    if (filter.scope.has_value() &&
        (!filter.scope->is_valid() || filter.scope->value() >= model.scopes.size())) {
        return false;
    }
    for (const auto kind : filter.relation_kinds) {
        if (!valid_relation_kind(kind)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool add_usage(std::size_t &target, std::size_t value) noexcept {
    if (value > std::numeric_limits<std::size_t>::max() - target) {
        target = std::numeric_limits<std::size_t>::max();
        return false;
    }
    target += value;
    return true;
}

[[nodiscard]] TraversalState run_traversal(const NormalizedModel &model,
                                           const AvailabilitySnapshot &snapshot, ObjectRef source,
                                           const GraphFilter &filter, const TraversalBudget &budget,
                                           const Exclusions &exclusions,
                                           std::optional<ObjectRef> target = std::nullopt) {
    TraversalState result;
    result.reached.resize(object_count(model), false);
    result.predecessor.resize(object_count(model));
    result.predecessor_relation.resize(object_count(model));

    if (!snapshot_matches_model(model, snapshot) || !valid_object(model, source) ||
        (target.has_value() && !valid_object(model, *target)) ||
        exclusions.objects.size() != object_count(model) ||
        exclusions.relations.size() != model.relations.size() || !valid_filter(model, filter)) {
        return result;
    }
    const auto domain_mask = failure_domain_mask(model, filter);
    if (!domain_mask.has_value()) {
        return result;
    }
    if (is_excluded(model, source, exclusions) ||
        !object_matches_filter(model, snapshot, source, filter, *domain_mask) ||
        (target.has_value() &&
         (is_excluded(model, *target, exclusions) ||
          !object_matches_filter(model, snapshot, *target, filter, *domain_mask)))) {
        result.status = AnalysisStatus::complete;
        return result;
    }
    if (budget.max_visited_objects == 0) {
        result.status = AnalysisStatus::incomplete;
        return result;
    }

    std::deque<ObjectRef> ready;
    const auto source_ordinal = object_ordinal(model, source);
    result.reached[source_ordinal] = true;
    result.discovered.push_back(source);
    result.usage.visited_objects = 1;
    ready.push_back(source);
    if (target.has_value() && source == *target) {
        result.status = AnalysisStatus::complete;
        return result;
    }

    while (!ready.empty()) {
        const auto current = ready.front();
        ready.pop_front();
        const auto *adjacency = object_adjacency(model, current);
        if (adjacency == nullptr) {
            result.status = AnalysisStatus::invalid;
            return result;
        }

        for (std::size_t kind_index = 0; kind_index < kRelationKindCount; ++kind_index) {
            const auto kind = static_cast<RelationKind>(kind_index);
            if (!relation_kind_selected(filter, kind)) {
                continue;
            }
            const auto process_range = [&](IndexRange range) {
                if (!valid_range(range, model.relation_adjacency_entries.size())) {
                    result.status = AnalysisStatus::invalid;
                    return false;
                }
                for (std::uint32_t offset = 0; offset < range.count; ++offset) {
                    if (result.usage.examined_relations >= budget.max_examined_relations) {
                        result.status = AnalysisStatus::incomplete;
                        return false;
                    }
                    ++result.usage.examined_relations;
                    const auto &entry = model.relation_adjacency_entries[range.begin + offset];
                    if (!entry.relation.is_valid() ||
                        entry.relation.value() >= model.relations.size() ||
                        !valid_object(model, entry.object)) {
                        result.status = AnalysisStatus::invalid;
                        return false;
                    }
                    const auto &relation = model.relations[entry.relation.value()];
                    if (relation.id != entry.relation || relation.kind != kind ||
                        exclusions.relations[entry.relation.value()] ||
                        is_excluded(model, entry.object, exclusions) ||
                        !object_matches_filter(model, snapshot, entry.object, filter,
                                               *domain_mask)) {
                        continue;
                    }
                    const auto ordinal = object_ordinal(model, entry.object);
                    if (result.reached[ordinal]) {
                        continue;
                    }
                    if (result.usage.visited_objects >= budget.max_visited_objects) {
                        result.status = AnalysisStatus::incomplete;
                        return false;
                    }
                    result.reached[ordinal] = true;
                    result.predecessor[ordinal] = current;
                    result.predecessor_relation[ordinal] = entry.relation;
                    result.discovered.push_back(entry.object);
                    ++result.usage.visited_objects;
                    if (target.has_value() && entry.object == *target) {
                        result.status = AnalysisStatus::complete;
                        return false;
                    }
                    ready.push_back(entry.object);
                }
                return true;
            };

            if (filter.direction != TraversalDirection::reverse &&
                !process_range(adjacency->forward[kind_index])) {
                if (result.status == AnalysisStatus::complete && target.has_value() &&
                    result.reached[object_ordinal(model, *target)]) {
                    return result;
                }
                return result;
            }
            if (filter.direction != TraversalDirection::forward &&
                !process_range(adjacency->reverse[kind_index])) {
                if (result.status == AnalysisStatus::complete && target.has_value() &&
                    result.reached[object_ordinal(model, *target)]) {
                    return result;
                }
                return result;
            }
        }
    }
    result.status = AnalysisStatus::complete;
    return result;
}

[[nodiscard]] Exclusions no_exclusions(const NormalizedModel &model) {
    return {std::vector<bool>(object_count(model), false),
            std::vector<bool>(model.relations.size(), false)};
}

[[nodiscard]] bool prepare_removal(const NormalizedModel &model, RemovalTarget removal,
                                   Exclusions &exclusions, std::vector<ObjectRef> &objects,
                                   std::vector<RelationId> &relations) {
    switch (removal.kind) {
    case RemovalKind::object:
        if (!valid_object(model, removal.object_value)) {
            return false;
        }
        exclusions.objects[object_ordinal(model, removal.object_value)] = true;
        objects.push_back(removal.object_value);
        break;
    case RemovalKind::relation:
        if (!removal.relation_value.is_valid() ||
            removal.relation_value.value() >= model.relations.size() ||
            model.relations[removal.relation_value.value()].id != removal.relation_value) {
            return false;
        }
        exclusions.relations[removal.relation_value.value()] = true;
        relations.push_back(removal.relation_value);
        break;
    case RemovalKind::failure_domain: {
        const auto expansion = expand_failure_domain(model, removal.failure_domain_value);
        if (!expansion.has_value()) {
            return false;
        }
        objects = expansion->members;
        for (const auto object : objects) {
            exclusions.objects[object_ordinal(model, object)] = true;
        }
        break;
    }
    }
    sort_objects(model, objects);
    return true;
}

} // namespace

bool AvailabilitySelection::includes(AvailabilityState state) const noexcept {
    switch (state) {
    case AvailabilityState::available:
        return available;
    case AvailabilityState::degraded:
        return degraded;
    case AvailabilityState::unavailable:
        return unavailable;
    case AvailabilityState::unknown:
        return unknown;
    }
    return false;
}

ReachabilityResult analyze_reachability(const NormalizedModel &model,
                                        const AvailabilitySnapshot &snapshot, ObjectRef source,
                                        const GraphFilter &filter, const TraversalBudget &budget) {
    ReachabilityResult result;
    result.source = source;
    const auto traversal =
        run_traversal(model, snapshot, source, filter, budget, no_exclusions(model));
    result.status = traversal.status;
    result.usage = traversal.usage;
    result.objects = traversal.discovered;
    sort_objects(model, result.objects);
    const auto qualification = qualify_objects(model, snapshot, result.objects);
    if (!qualification.has_value()) {
        result.status = AnalysisStatus::invalid;
        result.objects.clear();
        return result;
    }
    result.qualification = qualification->qualification;
    result.contains_unknown_availability = qualification->contains_unknown_availability;
    return result;
}

PathResult find_path(const NormalizedModel &model, const AvailabilitySnapshot &snapshot,
                     ObjectRef source, ObjectRef target, const GraphFilter &filter,
                     const TraversalBudget &budget) {
    PathResult result;
    result.source = source;
    result.target = target;
    const auto traversal =
        run_traversal(model, snapshot, source, filter, budget, no_exclusions(model), target);
    result.status = traversal.status;
    result.usage = traversal.usage;
    if (!valid_object(model, target) || !traversal.reached[object_ordinal(model, target)]) {
        return result;
    }

    result.reachable = true;
    auto current = target;
    while (current != source) {
        result.objects.push_back(current);
        const auto ordinal = object_ordinal(model, current);
        const auto relation = traversal.predecessor_relation[ordinal];
        const auto predecessor = traversal.predecessor[ordinal];
        if (!relation.is_valid() || !valid_object(model, predecessor)) {
            result.status = AnalysisStatus::invalid;
            result.reachable = false;
            result.objects.clear();
            result.relations.clear();
            return result;
        }
        result.relations.push_back(relation);
        current = predecessor;
    }
    result.objects.push_back(source);
    std::reverse(result.objects.begin(), result.objects.end());
    std::reverse(result.relations.begin(), result.relations.end());
    const auto qualification = qualify_objects(model, snapshot, result.objects);
    if (!qualification.has_value()) {
        result.status = AnalysisStatus::invalid;
        result.reachable = false;
        result.objects.clear();
        result.relations.clear();
        return result;
    }
    result.qualification = qualification->qualification;
    result.contains_unknown_availability = qualification->contains_unknown_availability;
    return result;
}

ImpactResult analyze_removal_impact(const NormalizedModel &model,
                                    const AvailabilitySnapshot &snapshot, ObjectRef source,
                                    RemovalTarget removal, const GraphFilter &filter,
                                    const TraversalBudget &budget) {
    ImpactResult result;
    result.source = source;
    result.removal = removal;
    auto exclusions = no_exclusions(model);
    if (!prepare_removal(model, removal, exclusions, result.removed_objects,
                         result.removed_relations)) {
        return result;
    }

    const auto baseline =
        run_traversal(model, snapshot, source, filter, budget, no_exclusions(model));
    const auto changed = run_traversal(model, snapshot, source, filter, budget, exclusions);
    result.status =
        baseline.status == AnalysisStatus::invalid || changed.status == AnalysisStatus::invalid
            ? AnalysisStatus::invalid
        : baseline.status == AnalysisStatus::incomplete ||
                changed.status == AnalysisStatus::incomplete
            ? AnalysisStatus::incomplete
            : AnalysisStatus::complete;
    result.usage = baseline.usage;
    if (!add_usage(result.usage.visited_objects, changed.usage.visited_objects) ||
        !add_usage(result.usage.examined_relations, changed.usage.examined_relations)) {
        result.status = AnalysisStatus::incomplete;
    }
    if (result.status != AnalysisStatus::complete) {
        return result;
    }

    for (const auto object : baseline.discovered) {
        if (!changed.reached[object_ordinal(model, object)]) {
            result.affected_objects.push_back(object);
        }
    }
    sort_objects(model, result.affected_objects);
    const auto qualification = qualify_objects(model, snapshot, result.affected_objects);
    if (!qualification.has_value()) {
        result.status = AnalysisStatus::invalid;
        result.affected_objects.clear();
        return result;
    }
    result.qualification = qualification->qualification;
    result.contains_unknown_availability = qualification->contains_unknown_availability;
    return result;
}

} // namespace ltfa
