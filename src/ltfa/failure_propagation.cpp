#include "ltfa/failure_propagation.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <optional>
#include <queue>
#include <string>
#include <tuple>
#include <utility>

namespace ltfa {
namespace {

enum class WorkKind {
    injection,
    group_transition,
    recompute,
};

struct WorkEvent {
    std::uint64_t effective_time_ms{0};
    std::string stable_id;
    WorkKind kind{WorkKind::injection};
    ObjectRef object;
    RequirementGroupId requirement_group;
    PropagationCause cause;
    std::size_t sequence{0};
};

struct WorkLater {
    [[nodiscard]] bool operator()(const WorkEvent &left, const WorkEvent &right) const noexcept {
        return std::tie(left.effective_time_ms, left.stable_id, left.kind, left.sequence) >
               std::tie(right.effective_time_ms, right.stable_id, right.kind, right.sequence);
    }
};

struct ResolvedInjection {
    ObjectRef object;
    RelationId relation;
    FailureDomainId failure_domain;
    AvailabilityState state{AvailabilityState::unavailable};
    std::uint64_t delay_ms{0};
    std::string stable_id;
    bool unresolved_failure_mode{false};
};

[[nodiscard]] std::size_t object_count(const NormalizedModel &model) noexcept {
    return model.entities.size() + model.endpoints.size();
}

[[nodiscard]] bool valid_object(const NormalizedModel &model, ObjectRef object) noexcept {
    if (!object.is_valid()) {
        return false;
    }
    return object.kind == ObjectKind::entity ? object.index < model.entities.size()
                                             : object.index < model.endpoints.size();
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

[[nodiscard]] bool valid_failure_mode_kind(FailureModeKind kind) noexcept {
    return static_cast<std::size_t>(kind) <= static_cast<std::size_t>(FailureModeKind::extension);
}

[[nodiscard]] const ObjectAvailabilityAdjacency *
availability_adjacency(const NormalizedModel &model, ObjectRef object) noexcept {
    if (!valid_object(model, object)) {
        return nullptr;
    }
    if (object.kind == ObjectKind::entity) {
        if (object.index >= model.entity_availability_adjacency.size()) {
            return nullptr;
        }
        return &model.entity_availability_adjacency[object.index];
    }
    if (object.index >= model.endpoint_availability_adjacency.size()) {
        return nullptr;
    }
    return &model.endpoint_availability_adjacency[object.index];
}

[[nodiscard]] std::optional<AssuranceQualification>
object_qualification(const NormalizedModel &model, ObjectRef object) noexcept {
    if (!valid_object(model, object)) {
        return std::nullopt;
    }
    const auto scope = object.kind == ObjectKind::entity ? model.entities[object.index].scope
                                                         : model.endpoints[object.index].scope;
    if (!scope.is_valid() || scope.value() >= model.scopes.size()) {
        return std::nullopt;
    }
    const auto &value = model.scopes[scope.value()];
    return AssuranceQualification{value.completeness, value.evidence_quality};
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

[[nodiscard]] bool snapshot_matches_model(const NormalizedModel &model,
                                          const AvailabilitySnapshot &snapshot) noexcept {
    return snapshot.entity_states.size() == model.entities.size() &&
           snapshot.endpoint_states.size() == model.endpoints.size();
}

[[nodiscard]] bool validate_normalized_dependencies(const NormalizedModel &model) {
    if (model.entity_availability_adjacency.size() != model.entities.size() ||
        model.endpoint_availability_adjacency.size() != model.endpoints.size()) {
        return false;
    }

    std::vector<std::size_t> forward_counts(model.availability_dependencies.size(), 0);
    std::vector<std::size_t> reverse_counts(model.availability_dependencies.size(), 0);
    for (const auto &dependency : model.availability_dependencies) {
        if (!dependency.id.is_valid() ||
            dependency.id.value() >= model.availability_dependencies.size() ||
            model.availability_dependencies[dependency.id.value()].id != dependency.id ||
            !valid_object(model, dependency.provider) ||
            !valid_object(model, dependency.consumer) ||
            dependency.relation.is_valid() == dependency.requirement_group.is_valid()) {
            return false;
        }
        if (dependency.relation.is_valid() &&
            (dependency.relation.value() >= model.relations.size() ||
             model.relations[dependency.relation.value()].id != dependency.relation)) {
            return false;
        }
        if (dependency.requirement_group.is_valid() &&
            (dependency.requirement_group.value() >= model.requirement_groups.size() ||
             model.requirement_groups[dependency.requirement_group.value()].id !=
                 dependency.requirement_group)) {
            return false;
        }
    }

    for (std::size_t ordinal = 0; ordinal < object_count(model); ++ordinal) {
        const auto object = object_from_ordinal(model, ordinal);
        const auto *adjacency = availability_adjacency(model, object);
        if (adjacency == nullptr ||
            !valid_range(adjacency->forward, model.availability_adjacency_entries.size()) ||
            !valid_range(adjacency->reverse, model.availability_adjacency_entries.size())) {
            return false;
        }
        for (std::uint32_t offset = 0; offset < adjacency->forward.count; ++offset) {
            const auto &entry =
                model.availability_adjacency_entries[adjacency->forward.begin + offset];
            if (!entry.dependency.is_valid() ||
                entry.dependency.value() >= model.availability_dependencies.size()) {
                return false;
            }
            const auto &dependency = model.availability_dependencies[entry.dependency.value()];
            if (dependency.provider != object || dependency.consumer != entry.object) {
                return false;
            }
            ++forward_counts[entry.dependency.value()];
        }
        for (std::uint32_t offset = 0; offset < adjacency->reverse.count; ++offset) {
            const auto &entry =
                model.availability_adjacency_entries[adjacency->reverse.begin + offset];
            if (!entry.dependency.is_valid() ||
                entry.dependency.value() >= model.availability_dependencies.size()) {
                return false;
            }
            const auto &dependency = model.availability_dependencies[entry.dependency.value()];
            if (dependency.consumer != object || dependency.provider != entry.object) {
                return false;
            }
            ++reverse_counts[entry.dependency.value()];
        }
    }
    for (std::size_t index = 0; index < forward_counts.size(); ++index) {
        if (forward_counts[index] != 1 || reverse_counts[index] != 1) {
            return false;
        }
    }

    std::vector<std::size_t> indegree(object_count(model), 0);
    for (const auto &dependency : model.availability_dependencies) {
        ++indegree[object_ordinal(model, dependency.consumer)];
    }
    std::deque<ObjectRef> ready;
    for (std::size_t ordinal = 0; ordinal < indegree.size(); ++ordinal) {
        if (indegree[ordinal] == 0) {
            ready.push_back(object_from_ordinal(model, ordinal));
        }
    }
    std::size_t visited = 0;
    while (!ready.empty()) {
        const auto object = ready.front();
        ready.pop_front();
        ++visited;
        const auto *adjacency = availability_adjacency(model, object);
        for (std::uint32_t offset = 0; offset < adjacency->forward.count; ++offset) {
            const auto &entry =
                model.availability_adjacency_entries[adjacency->forward.begin + offset];
            auto &consumer_indegree = indegree[object_ordinal(model, entry.object)];
            if (consumer_indegree == 0) {
                return false;
            }
            --consumer_indegree;
            if (consumer_indegree == 0) {
                ready.push_back(entry.object);
            }
        }
    }
    return visited == object_count(model);
}

[[nodiscard]] bool validate_requirements(const NormalizedModel &model,
                                         const AvailabilitySnapshot &snapshot) {
    for (const auto &group : model.requirement_groups) {
        if (!group.id.is_valid() || group.id.value() >= model.requirement_groups.size() ||
            model.requirement_groups[group.id.value()].id != group.id ||
            !group.consumer.is_valid() || group.consumer.value() >= model.entities.size() ||
            !valid_range(group.providers, model.requirement_providers.size()) ||
            !evaluate_requirement_group(model, group.id, snapshot).has_value()) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool validate_probes(const NormalizedModel &model) noexcept {
    for (const auto &probe : model.probes) {
        if (!probe.id.is_valid() || probe.id.value() >= model.probes.size() ||
            model.probes[probe.id.value()].id != probe.id || !valid_object(model, probe.target) ||
            !valid_object(model, probe.vantage)) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::optional<ResolvedInjection> resolve_injection(const NormalizedModel &model,
                                                                 FailureInjection injection) {
    ResolvedInjection result;
    switch (injection.kind) {
    case FailureInjectionKind::object:
        if (!valid_object(model, injection.object_value) ||
            !valid_availability_state(injection.object_state) ||
            injection.object_state == AvailabilityState::available) {
            return std::nullopt;
        }
        result.object = injection.object_value;
        result.state = injection.object_state;
        result.delay_ms = injection.delay_ms;
        result.stable_id = std::string{model.external_id(result.object)};
        return result;
    case FailureInjectionKind::relation:
        if (!injection.relation_value.is_valid() ||
            injection.relation_value.value() >= model.relations.size() ||
            model.relations[injection.relation_value.value()].id != injection.relation_value) {
            return std::nullopt;
        }
        result.relation = injection.relation_value;
        result.delay_ms = injection.delay_ms;
        result.stable_id =
            std::string{model.string(model.relations[result.relation.value()].external_id)};
        return result;
    case FailureInjectionKind::failure_domain: {
        const auto expansion = expand_failure_domain(model, injection.failure_domain_value);
        if (!expansion.has_value()) {
            return std::nullopt;
        }
        result.failure_domain = injection.failure_domain_value;
        result.delay_ms = injection.delay_ms;
        result.stable_id = std::string{
            model.string(model.failure_domains[result.failure_domain.value()].external_id)};
        return result;
    }
    case FailureInjectionKind::declared_mode:
        if (!injection.failure_mode_value.is_valid() ||
            injection.failure_mode_value.value() >= model.failure_modes.size()) {
            return std::nullopt;
        }
        {
            const auto &mode = model.failure_modes[injection.failure_mode_value.value()];
            if (mode.id != injection.failure_mode_value || !valid_object(model, mode.target) ||
                !valid_failure_mode_kind(mode.kind)) {
                return std::nullopt;
            }
            result.object = mode.target;
            result.state = failure_mode_result(mode.kind);
            result.delay_ms = mode.delay_ms;
            result.stable_id = std::string{model.string(mode.external_id)};
            result.unresolved_failure_mode = mode.kind == FailureModeKind::extension;
            return result;
        }
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<AvailabilityState>
compute_object_state(const NormalizedModel &model, ObjectRef object,
                     const AvailabilitySnapshot &intrinsic, const AvailabilitySnapshot &current,
                     const std::vector<AvailabilityState> &group_states,
                     const std::vector<std::vector<RequirementGroupId>> &consumer_groups,
                     const std::vector<bool> &failed_relations) {
    const auto *adjacency = availability_adjacency(model, object);
    if (adjacency == nullptr ||
        !valid_range(adjacency->reverse, model.availability_adjacency_entries.size())) {
        return std::nullopt;
    }

    std::vector<AvailabilityState> required_states;
    required_states.push_back(intrinsic.state(object));
    for (std::uint32_t offset = 0; offset < adjacency->reverse.count; ++offset) {
        const auto &entry = model.availability_adjacency_entries[adjacency->reverse.begin + offset];
        if (!entry.dependency.is_valid() ||
            entry.dependency.value() >= model.availability_dependencies.size()) {
            return std::nullopt;
        }
        const auto &dependency = model.availability_dependencies[entry.dependency.value()];
        if (!dependency.relation.is_valid()) {
            continue;
        }
        if (dependency.relation.value() >= failed_relations.size()) {
            return std::nullopt;
        }
        required_states.push_back(failed_relations[dependency.relation.value()]
                                      ? AvailabilityState::unavailable
                                      : current.state(dependency.provider));
    }
    for (const auto group : consumer_groups[object_ordinal(model, object)]) {
        if (!group.is_valid() || group.value() >= group_states.size()) {
            return std::nullopt;
        }
        required_states.push_back(group_states[group.value()]);
    }
    return evaluate_requirement_states(RequirementMode::all, required_states);
}

[[nodiscard]] bool checked_add(std::uint64_t left, std::uint64_t right,
                               std::uint64_t &result) noexcept {
    if (right > std::numeric_limits<std::uint64_t>::max() - left) {
        return false;
    }
    result = left + right;
    return true;
}

void classify_result(const NormalizedModel &model, PropagationResult &result) {
    std::vector<bool> changed(object_count(model), false);
    std::vector<std::uint64_t> final_transition_time(object_count(model), 0);
    for (const auto &event : result.events) {
        const auto ordinal = object_ordinal(model, event.object);
        changed[ordinal] = true;
        final_transition_time[ordinal] = event.effective_time_ms;
    }

    result.qualification = {Completeness::verified, EvidenceQuality::confirmed};
    bool has_qualified_object = false;
    for (std::size_t ordinal = 0; ordinal < object_count(model); ++ordinal) {
        const auto object = object_from_ordinal(model, ordinal);
        const auto state = result.final_snapshot.state(object);
        if (!changed[ordinal]) {
            result.unaffected_objects.push_back(object);
        } else {
            const auto qualification = object_qualification(model, object);
            if (!qualification.has_value()) {
                result.status = PropagationStatus::invalid;
                return;
            }
            combine_qualification(result.qualification, *qualification);
            has_qualified_object = true;
            if (state == AvailabilityState::unavailable) {
                auto &objects = final_transition_time[ordinal] == 0 ? result.immediate_unavailable
                                                                    : result.delayed_unavailable;
                objects.push_back(object);
            } else if (state == AvailabilityState::degraded) {
                auto &objects = final_transition_time[ordinal] == 0 ? result.immediate_degraded
                                                                    : result.delayed_degraded;
                objects.push_back(object);
            }

            const bool has_probe =
                std::any_of(model.probes.begin(), model.probes.end(),
                            [&](const Probe &probe) { return probe.target == object; });
            if (!has_probe) {
                result.detection_gaps.push_back(object);
            }
        }
        if (state == AvailabilityState::unknown) {
            result.unknown_objects.push_back(object);
        }
    }
    if (!has_qualified_object) {
        result.qualification = {Completeness::unknown, EvidenceQuality::unknown};
    }

    for (const auto &probe : model.probes) {
        if (result.final_snapshot.state(probe.target) == AvailabilityState::unavailable ||
            result.final_snapshot.state(probe.vantage) == AvailabilityState::unavailable) {
            result.lost_probes.push_back(probe.id);
        }
    }

    sort_objects(model, result.immediate_unavailable);
    sort_objects(model, result.immediate_degraded);
    sort_objects(model, result.delayed_unavailable);
    sort_objects(model, result.delayed_degraded);
    sort_objects(model, result.unaffected_objects);
    sort_objects(model, result.unknown_objects);
    sort_objects(model, result.detection_gaps);
    std::sort(result.lost_probes.begin(), result.lost_probes.end(),
              [&](ProbeId left, ProbeId right) {
                  return model.string(model.probes[left.value()].external_id) <
                         model.string(model.probes[right.value()].external_id);
              });
}

} // namespace

AvailabilityState failure_mode_result(FailureModeKind mode) noexcept {
    switch (mode) {
    case FailureModeKind::undervoltage:
    case FailureModeKind::termination_fault:
    case FailureModeKind::noise:
    case FailureModeKind::logic_inverted:
    case FailureModeKind::under_range:
    case FailureModeKind::over_range:
    case FailureModeKind::value_frozen:
    case FailureModeKind::heartbeat_stale:
        return AvailabilityState::degraded;
    case FailureModeKind::extension:
        return AvailabilityState::unknown;
    case FailureModeKind::power_loss:
    case FailureModeKind::breaker_trip:
    case FailureModeKind::psu_loss:
    case FailureModeKind::ups_depleted:
    case FailureModeKind::link_down:
    case FailureModeKind::port_down:
    case FailureModeKind::device_failure:
    case FailureModeKind::route_loss:
    case FailureModeKind::bus_open:
    case FailureModeKind::bus_short:
    case FailureModeKind::master_loss:
    case FailureModeKind::stuck_open:
    case FailureModeKind::stuck_closed:
    case FailureModeKind::wetting_power_loss:
    case FailureModeKind::loop_open:
    case FailureModeKind::sensor_power_loss:
    case FailureModeKind::tunnel_down:
    case FailureModeKind::peer_unreachable:
    case FailureModeKind::security_association_loss:
    case FailureModeKind::heartbeat_failed:
    case FailureModeKind::probe_path_loss:
        return AvailabilityState::unavailable;
    }
    return AvailabilityState::unknown;
}

PropagationResult propagate_failure(const NormalizedModel &model,
                                    const AvailabilitySnapshot &initial, FailureInjection injection,
                                    const PropagationBudget &budget) {
    PropagationResult result;
    result.injection = injection;
    result.final_snapshot = initial;

    if (!snapshot_matches_model(model, initial) || !validate_normalized_dependencies(model) ||
        !validate_requirements(model, initial) || !validate_probes(model)) {
        return result;
    }
    const auto resolved = resolve_injection(model, injection);
    if (!resolved.has_value()) {
        return result;
    }
    result.unresolved_failure_mode = resolved->unresolved_failure_mode;

    AvailabilitySnapshot intrinsic = initial;
    std::vector<bool> failed_relations(model.relations.size(), false);
    std::vector<AvailabilityState> group_states;
    group_states.reserve(model.requirement_groups.size());
    std::vector<std::vector<RequirementGroupId>> consumer_groups(object_count(model));
    for (const auto &group : model.requirement_groups) {
        const auto evaluation = evaluate_requirement_group(model, group.id, result.final_snapshot);
        if (!evaluation.has_value()) {
            return result;
        }
        group_states.push_back(evaluation->state);
        consumer_groups[object_ordinal(model, ObjectRef::from(group.consumer))].push_back(group.id);
    }

    std::priority_queue<WorkEvent, std::vector<WorkEvent>, WorkLater> work;
    std::vector<std::optional<std::uint64_t>> pending_group_times(model.requirement_groups.size());
    bool event_budget_exhausted = false;
    std::size_t next_sequence = 0;
    const auto schedule = [&](WorkEvent event) {
        if (result.usage.scheduled_events >= budget.max_events) {
            event_budget_exhausted = true;
            return false;
        }
        event.sequence = next_sequence++;
        work.push(std::move(event));
        ++result.usage.scheduled_events;
        return true;
    };

    if (!schedule({resolved->delay_ms, resolved->stable_id, WorkKind::injection, {}, {}, {}})) {
        result.status = PropagationStatus::incomplete;
        return result;
    }

    result.status = PropagationStatus::complete;
    while (!work.empty() && !event_budget_exhausted) {
        auto event = work.top();
        work.pop();
        ++result.usage.processed_events;

        if (event.kind == WorkKind::injection) {
            if (resolved->relation.is_valid()) {
                failed_relations[resolved->relation.value()] = true;
                result.failed_relations.push_back(resolved->relation);
                for (const auto &dependency : model.availability_dependencies) {
                    if (dependency.relation == resolved->relation &&
                        !schedule(
                            {event.effective_time_ms,
                             std::string{model.external_id(dependency.consumer)},
                             WorkKind::recompute,
                             dependency.consumer,
                             {},
                             {PropagationCauseKind::availability_dependency, dependency.id, {}}})) {
                        break;
                    }
                }
                continue;
            }

            std::vector<ObjectRef> injected_objects;
            if (resolved->failure_domain.is_valid()) {
                const auto expansion = expand_failure_domain(model, resolved->failure_domain);
                if (!expansion.has_value()) {
                    result.status = PropagationStatus::invalid;
                    break;
                }
                injected_objects = expansion->members;
            } else {
                injected_objects.push_back(resolved->object);
            }
            for (const auto object : injected_objects) {
                if (!intrinsic.set(object, resolved->state) ||
                    !schedule({event.effective_time_ms,
                               std::string{model.external_id(object)},
                               WorkKind::recompute,
                               object,
                               {},
                               {PropagationCauseKind::injection, {}, {}}})) {
                    if (!event_budget_exhausted) {
                        result.status = PropagationStatus::invalid;
                    }
                    break;
                }
            }
            if (result.status == PropagationStatus::invalid) {
                break;
            }
            continue;
        }

        if (event.kind == WorkKind::group_transition) {
            if (!event.requirement_group.is_valid() ||
                event.requirement_group.value() >= model.requirement_groups.size()) {
                result.status = PropagationStatus::invalid;
                break;
            }
            auto &pending = pending_group_times[event.requirement_group.value()];
            if (!pending.has_value() || *pending != event.effective_time_ms) {
                continue;
            }
            pending.reset();
            const auto evaluation =
                evaluate_requirement_group(model, event.requirement_group, result.final_snapshot);
            if (!evaluation.has_value()) {
                result.status = PropagationStatus::invalid;
                break;
            }
            auto &effective = group_states[event.requirement_group.value()];
            if (effective != evaluation->state) {
                effective = evaluation->state;
                const auto consumer = ObjectRef::from(
                    model.requirement_groups[event.requirement_group.value()].consumer);
                if (!schedule(
                        {event.effective_time_ms,
                         std::string{model.external_id(consumer)},
                         WorkKind::recompute,
                         consumer,
                         {},
                         {PropagationCauseKind::requirement_group, {}, event.requirement_group}})) {
                    break;
                }
            }
            continue;
        }

        const auto computed =
            compute_object_state(model, event.object, intrinsic, result.final_snapshot,
                                 group_states, consumer_groups, failed_relations);
        if (!computed.has_value()) {
            result.status = PropagationStatus::invalid;
            break;
        }
        const auto previous = result.final_snapshot.state(event.object);
        if (previous == *computed) {
            continue;
        }
        if (result.usage.state_transitions >= budget.max_state_transitions) {
            result.status = PropagationStatus::incomplete;
            break;
        }
        const auto qualification = object_qualification(model, event.object);
        if (!qualification.has_value() || !result.final_snapshot.set(event.object, *computed)) {
            result.status = PropagationStatus::invalid;
            break;
        }
        result.events.push_back({event.effective_time_ms, event.object, previous, *computed,
                                 event.cause, *qualification});
        ++result.usage.state_transitions;

        const auto *adjacency = availability_adjacency(model, event.object);
        for (std::uint32_t offset = 0; offset < adjacency->forward.count; ++offset) {
            const auto &entry =
                model.availability_adjacency_entries[adjacency->forward.begin + offset];
            const auto &dependency = model.availability_dependencies[entry.dependency.value()];
            if (dependency.requirement_group.is_valid()) {
                const auto evaluation = evaluate_requirement_group(
                    model, dependency.requirement_group, result.final_snapshot);
                if (!evaluation.has_value()) {
                    result.status = PropagationStatus::invalid;
                    break;
                }
                const auto group_index = dependency.requirement_group.value();
                if (group_states[group_index] == evaluation->state) {
                    continue;
                }
                const auto &group = model.requirement_groups[group_index];
                if (group.hold_up_ms == 0) {
                    group_states[group_index] = evaluation->state;
                    const auto consumer = ObjectRef::from(group.consumer);
                    if (!schedule({event.effective_time_ms,
                                   std::string{model.external_id(consumer)},
                                   WorkKind::recompute,
                                   consumer,
                                   {},
                                   {PropagationCauseKind::requirement_group,
                                    {},
                                    dependency.requirement_group}})) {
                        break;
                    }
                } else if (!pending_group_times[group_index].has_value()) {
                    std::uint64_t effective_time = 0;
                    if (!checked_add(event.effective_time_ms, group.hold_up_ms, effective_time)) {
                        result.status = PropagationStatus::invalid;
                        break;
                    }
                    pending_group_times[group_index] = effective_time;
                    if (!schedule({effective_time,
                                   std::string{model.string(group.external_id)},
                                   WorkKind::group_transition,
                                   {},
                                   dependency.requirement_group,
                                   {PropagationCauseKind::requirement_group,
                                    {},
                                    dependency.requirement_group}})) {
                        break;
                    }
                }
            } else if (!schedule(
                           {event.effective_time_ms,
                            std::string{model.external_id(dependency.consumer)},
                            WorkKind::recompute,
                            dependency.consumer,
                            {},
                            {PropagationCauseKind::availability_dependency, dependency.id, {}}})) {
                break;
            }
        }
        if (result.status == PropagationStatus::invalid) {
            break;
        }
    }

    if (event_budget_exhausted) {
        result.status = PropagationStatus::incomplete;
    }
    std::stable_sort(result.events.begin(), result.events.end(),
                     [&](const PropagationEvent &left, const PropagationEvent &right) {
                         if (left.effective_time_ms != right.effective_time_ms) {
                             return left.effective_time_ms < right.effective_time_ms;
                         }
                         return model.external_id(left.object) < model.external_id(right.object);
                     });
    classify_result(model, result);
    return result;
}

} // namespace ltfa
