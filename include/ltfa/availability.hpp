#pragma once

#include "ltfa/model.hpp"

#include <optional>
#include <vector>

namespace ltfa {

struct AvailabilitySnapshot {
    std::vector<AvailabilityState> entity_states;
    std::vector<AvailabilityState> endpoint_states;

    [[nodiscard]] AvailabilityState state(ObjectRef object) const noexcept;
    [[nodiscard]] bool set(ObjectRef object, AvailabilityState value) noexcept;
};

struct AssuranceQualification {
    Completeness completeness{Completeness::unknown};
    EvidenceQuality evidence_quality{EvidenceQuality::unknown};
};

struct RequirementGroupEvaluation {
    RequirementGroupId group;
    AvailabilityState state{AvailabilityState::unknown};
    std::uint64_t hold_up_ms{0};
};

struct ConsumerRequirementEvaluation {
    EntityId consumer;
    AvailabilityState state{AvailabilityState::unknown};
    AssuranceQualification qualification;
    std::vector<RequirementGroupEvaluation> groups;
};

struct FailureDomainExpansion {
    FailureDomainId domain;
    AssuranceQualification qualification;
    std::vector<ObjectRef> members;
};

[[nodiscard]] AvailabilitySnapshot
make_availability_snapshot(const NormalizedModel &model,
                           AvailabilityState initial = AvailabilityState::available);

[[nodiscard]] AvailabilityState
evaluate_requirement_states(RequirementMode mode,
                            const std::vector<AvailabilityState> &provider_states,
                            std::uint32_t minimum_providers = 0) noexcept;

[[nodiscard]] std::optional<RequirementGroupEvaluation>
evaluate_requirement_group(const NormalizedModel &model, RequirementGroupId group,
                           const AvailabilitySnapshot &snapshot);

[[nodiscard]] std::optional<ConsumerRequirementEvaluation>
evaluate_consumer_requirements(const NormalizedModel &model, EntityId consumer,
                               const AvailabilitySnapshot &snapshot);

[[nodiscard]] std::optional<FailureDomainExpansion>
expand_failure_domain(const NormalizedModel &model, FailureDomainId domain);

[[nodiscard]] bool apply_failure_domain_failure(const NormalizedModel &model,
                                                FailureDomainId domain,
                                                AvailabilitySnapshot &snapshot) noexcept;

} // namespace ltfa
