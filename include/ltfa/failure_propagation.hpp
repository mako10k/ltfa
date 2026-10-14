// R: Define bounded single-failure and simultaneous-state propagation interfaces.
#pragma once

#include "ltfa/availability.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ltfa {

enum class PropagationStatus {
    complete,
    incomplete,
    invalid,
};

enum class FailureInjectionKind {
    object,
    relation,
    failure_domain,
    declared_mode,
};

struct FailureInjection {
    FailureInjectionKind kind{FailureInjectionKind::object};
    ObjectRef object_value;
    RelationId relation_value;
    FailureDomainId failure_domain_value;
    FailureModeId failure_mode_value;
    AvailabilityState object_state{AvailabilityState::unavailable};
    std::uint64_t delay_ms{0};

    [[nodiscard]] static constexpr FailureInjection
    for_object(ObjectRef object, AvailabilityState state = AvailabilityState::unavailable,
               std::uint64_t delay = 0) noexcept {
        FailureInjection value;
        value.kind = FailureInjectionKind::object;
        value.object_value = object;
        value.object_state = state;
        value.delay_ms = delay;
        return value;
    }

    [[nodiscard]] static constexpr FailureInjection for_relation(RelationId relation,
                                                                 std::uint64_t delay = 0) noexcept {
        FailureInjection value;
        value.kind = FailureInjectionKind::relation;
        value.relation_value = relation;
        value.delay_ms = delay;
        return value;
    }

    [[nodiscard]] static constexpr FailureInjection
    for_failure_domain(FailureDomainId domain, std::uint64_t delay = 0) noexcept {
        FailureInjection value;
        value.kind = FailureInjectionKind::failure_domain;
        value.failure_domain_value = domain;
        value.delay_ms = delay;
        return value;
    }

    [[nodiscard]] static constexpr FailureInjection for_declared_mode(FailureModeId mode) noexcept {
        FailureInjection value;
        value.kind = FailureInjectionKind::declared_mode;
        value.failure_mode_value = mode;
        return value;
    }
};

enum class PropagationCauseKind {
    injection,
    availability_dependency,
    requirement_group,
};

struct PropagationCause {
    PropagationCauseKind kind{PropagationCauseKind::injection};
    AvailabilityDependencyId dependency;
    RequirementGroupId requirement_group;
};

struct PropagationEvent {
    std::uint64_t effective_time_ms{0};
    ObjectRef object;
    AvailabilityState previous_state{AvailabilityState::unknown};
    AvailabilityState state{AvailabilityState::unknown};
    PropagationCause cause;
    AssuranceQualification qualification;
};

struct PropagationBudget {
    std::size_t max_events{1'000'000};
    std::size_t max_state_transitions{300'000};
};

struct PropagationUsage {
    std::size_t scheduled_events{0};
    std::size_t processed_events{0};
    std::size_t state_transitions{0};
};

struct PropagationResult {
    PropagationStatus status{PropagationStatus::invalid};
    FailureInjection injection;
    AvailabilitySnapshot final_snapshot;
    std::vector<PropagationEvent> events;
    std::vector<ObjectRef> immediate_unavailable;
    std::vector<ObjectRef> immediate_degraded;
    std::vector<ObjectRef> delayed_unavailable;
    std::vector<ObjectRef> delayed_degraded;
    std::vector<ObjectRef> unaffected_objects;
    std::vector<ObjectRef> unknown_objects;
    std::vector<ObjectRef> detection_gaps;
    std::vector<ProbeId> lost_probes;
    std::vector<RelationId> failed_relations;
    AssuranceQualification qualification;
    bool unresolved_failure_mode{false};
    PropagationUsage usage;
};

[[nodiscard]] AvailabilityState failure_mode_result(FailureModeKind mode) noexcept;

[[nodiscard]] PropagationResult propagate_failure(const NormalizedModel &model,
                                                  const AvailabilitySnapshot &initial,
                                                  FailureInjection injection,
                                                  const PropagationBudget &budget = {});

// R: Apply distinct object states simultaneously at time zero, then propagate
// dependencies with the same bounded engine. available remains subject to providers.
[[nodiscard]] PropagationResult propagate_states(const NormalizedModel &model,
                                                 const AvailabilitySnapshot &initial,
                                                 const std::vector<FailureInjection> &states,
                                                 const PropagationBudget &budget = {});

} // namespace ltfa
