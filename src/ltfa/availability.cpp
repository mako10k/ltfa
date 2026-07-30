#include "ltfa/availability.hpp"

#include <algorithm>
#include <cstddef>

namespace ltfa {
namespace {

class RequirementAccumulator {
  public:
    explicit RequirementAccumulator(RequirementMode mode,
                                    std::uint32_t minimum_providers = 0) noexcept
        : mode_(mode), minimum_providers_(minimum_providers) {}

    void add(AvailabilityState state) noexcept {
        if (mode_ == RequirementMode::at_least) {
            has_state_ = true;
            switch (state) {
            case AvailabilityState::available:
                ++available_;
                break;
            case AvailabilityState::degraded:
                ++degraded_;
                break;
            case AvailabilityState::unknown:
                ++unknown_;
                break;
            case AvailabilityState::unavailable:
                ++unavailable_;
                break;
            }
            return;
        }
        if (!has_state_) {
            state_ = state;
            has_state_ = true;
            return;
        }

        if (mode_ == RequirementMode::all) {
            if (state_ == AvailabilityState::unavailable ||
                state == AvailabilityState::unavailable) {
                state_ = AvailabilityState::unavailable;
            } else if (state_ == AvailabilityState::unknown ||
                       state == AvailabilityState::unknown) {
                state_ = AvailabilityState::unknown;
            } else if (state_ == AvailabilityState::degraded ||
                       state == AvailabilityState::degraded) {
                state_ = AvailabilityState::degraded;
            } else {
                state_ = AvailabilityState::available;
            }
            return;
        }

        if (state_ == AvailabilityState::available || state == AvailabilityState::available) {
            state_ = AvailabilityState::available;
        } else if (state_ == AvailabilityState::degraded || state == AvailabilityState::degraded) {
            state_ = AvailabilityState::degraded;
        } else if (state_ == AvailabilityState::unknown || state == AvailabilityState::unknown) {
            state_ = AvailabilityState::unknown;
        } else {
            state_ = AvailabilityState::unavailable;
        }
    }

    [[nodiscard]] AvailabilityState state() const noexcept {
        if (mode_ == RequirementMode::at_least) {
            if (!has_state_ || minimum_providers_ == 0) {
                return AvailabilityState::unknown;
            }
            if (available_ >= minimum_providers_) {
                return degraded_ != 0 || unavailable_ != 0 ? AvailabilityState::degraded
                                                           : AvailabilityState::available;
            }
            if (available_ + degraded_ >= minimum_providers_) {
                return AvailabilityState::degraded;
            }
            if (available_ + degraded_ + unknown_ >= minimum_providers_) {
                return AvailabilityState::unknown;
            }
            return AvailabilityState::unavailable;
        }
        return has_state_ ? state_ : AvailabilityState::unknown;
    }

  private:
    RequirementMode mode_;
    std::uint32_t minimum_providers_{0};
    std::uint32_t available_{0};
    std::uint32_t degraded_{0};
    std::uint32_t unknown_{0};
    std::uint32_t unavailable_{0};
    AvailabilityState state_{AvailabilityState::unknown};
    bool has_state_{false};
};

[[nodiscard]] bool valid_range(IndexRange range, std::size_t size) noexcept {
    return range.begin <= size && range.count <= size - range.begin;
}

[[nodiscard]] bool valid_object(const NormalizedModel &model, ObjectRef object) noexcept {
    if (!object.is_valid()) {
        return false;
    }
    return object.kind == ObjectKind::entity ? object.index < model.entities.size()
                                             : object.index < model.endpoints.size();
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

} // namespace

AvailabilityState AvailabilitySnapshot::state(ObjectRef object) const noexcept {
    if (!object.is_valid()) {
        return AvailabilityState::unknown;
    }
    if (object.kind == ObjectKind::entity) {
        return object.index < entity_states.size() ? entity_states[object.index]
                                                   : AvailabilityState::unknown;
    }
    return object.index < endpoint_states.size() ? endpoint_states[object.index]
                                                 : AvailabilityState::unknown;
}

bool AvailabilitySnapshot::set(ObjectRef object, AvailabilityState value) noexcept {
    if (!object.is_valid()) {
        return false;
    }
    if (object.kind == ObjectKind::entity) {
        if (object.index >= entity_states.size()) {
            return false;
        }
        entity_states[object.index] = value;
        return true;
    }
    if (object.index >= endpoint_states.size()) {
        return false;
    }
    endpoint_states[object.index] = value;
    return true;
}

AvailabilitySnapshot make_availability_snapshot(const NormalizedModel &model,
                                                AvailabilityState initial) {
    return {std::vector<AvailabilityState>(model.entities.size(), initial),
            std::vector<AvailabilityState>(model.endpoints.size(), initial)};
}

AvailabilityState evaluate_requirement_states(RequirementMode mode,
                                              const std::vector<AvailabilityState> &provider_states,
                                              std::uint32_t minimum_providers) noexcept {
    RequirementAccumulator evaluation{mode, minimum_providers};
    for (const auto state : provider_states) {
        evaluation.add(state);
    }
    return evaluation.state();
}

std::optional<RequirementGroupEvaluation>
evaluate_requirement_group(const NormalizedModel &model, RequirementGroupId group,
                           const AvailabilitySnapshot &snapshot) {
    if (!group.is_valid() || group.value() >= model.requirement_groups.size()) {
        return std::nullopt;
    }
    const auto &requirement = model.requirement_groups[group.value()];
    if (requirement.id != group ||
        !valid_range(requirement.providers, model.requirement_providers.size())) {
        return std::nullopt;
    }
    if ((requirement.mode == RequirementMode::at_least &&
         (requirement.minimum_providers == 0 ||
          requirement.minimum_providers > requirement.providers.count)) ||
        (requirement.mode != RequirementMode::at_least && requirement.minimum_providers != 0)) {
        return std::nullopt;
    }

    RequirementAccumulator evaluation{requirement.mode, requirement.minimum_providers};
    for (std::uint32_t offset = 0; offset < requirement.providers.count; ++offset) {
        const auto provider = model.requirement_providers[requirement.providers.begin + offset];
        if (!provider.is_valid() || provider.value() >= model.entities.size()) {
            return std::nullopt;
        }
        evaluation.add(snapshot.state(ObjectRef::from(provider)));
    }
    return RequirementGroupEvaluation{group, evaluation.state(), requirement.hold_up_ms};
}

std::optional<ConsumerRequirementEvaluation>
evaluate_consumer_requirements(const NormalizedModel &model, EntityId consumer,
                               const AvailabilitySnapshot &snapshot) {
    if (!consumer.is_valid() || consumer.value() >= model.entities.size()) {
        return std::nullopt;
    }
    const auto qualification = object_qualification(model, ObjectRef::from(consumer));
    if (!qualification.has_value()) {
        return std::nullopt;
    }

    ConsumerRequirementEvaluation result;
    result.consumer = consumer;
    result.state = AvailabilityState::available;
    result.qualification = *qualification;

    RequirementAccumulator combined{RequirementMode::all};
    for (const auto &group : model.requirement_groups) {
        if (group.consumer != consumer) {
            continue;
        }
        const auto evaluation = evaluate_requirement_group(model, group.id, snapshot);
        if (!evaluation.has_value()) {
            return std::nullopt;
        }
        combined.add(evaluation->state);
        result.groups.push_back(*evaluation);
    }
    if (!result.groups.empty()) {
        result.state = combined.state();
    }
    return result;
}

std::optional<FailureDomainExpansion> expand_failure_domain(const NormalizedModel &model,
                                                            FailureDomainId domain) {
    if (!domain.is_valid() || domain.value() >= model.failure_domains.size()) {
        return std::nullopt;
    }
    const auto &failure_domain = model.failure_domains[domain.value()];
    if (failure_domain.id != domain ||
        !valid_range(failure_domain.members, model.failure_domain_members.size())) {
        return std::nullopt;
    }

    FailureDomainExpansion result;
    result.domain = domain;
    result.qualification = {Completeness::verified, EvidenceQuality::confirmed};
    for (std::uint32_t offset = 0; offset < failure_domain.members.count; ++offset) {
        const auto member = model.failure_domain_members[failure_domain.members.begin + offset];
        const auto qualification = object_qualification(model, member);
        if (!qualification.has_value()) {
            return std::nullopt;
        }
        result.members.push_back(member);
        combine_qualification(result.qualification, *qualification);
    }
    if (result.members.empty()) {
        result.qualification = {Completeness::unknown, EvidenceQuality::unknown};
        return result;
    }

    std::sort(result.members.begin(), result.members.end(), [&](ObjectRef left, ObjectRef right) {
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
    result.members.erase(std::unique(result.members.begin(), result.members.end()),
                         result.members.end());
    return result;
}

bool apply_failure_domain_failure(const NormalizedModel &model, FailureDomainId domain,
                                  AvailabilitySnapshot &snapshot) noexcept {
    if (!domain.is_valid() || domain.value() >= model.failure_domains.size() ||
        snapshot.entity_states.size() != model.entities.size() ||
        snapshot.endpoint_states.size() != model.endpoints.size()) {
        return false;
    }
    const auto &failure_domain = model.failure_domains[domain.value()];
    if (failure_domain.id != domain ||
        !valid_range(failure_domain.members, model.failure_domain_members.size())) {
        return false;
    }
    for (std::uint32_t offset = 0; offset < failure_domain.members.count; ++offset) {
        const auto member = model.failure_domain_members[failure_domain.members.begin + offset];
        if (!valid_object(model, member)) {
            return false;
        }
    }
    for (std::uint32_t offset = 0; offset < failure_domain.members.count; ++offset) {
        const auto member = model.failure_domain_members[failure_domain.members.begin + offset];
        if (!snapshot.set(member, AvailabilityState::unavailable)) {
            return false;
        }
    }
    return true;
}

} // namespace ltfa
