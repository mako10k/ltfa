#pragma once

// R: immutable physical-to-logical Turbo Ring forwarding analysis.
#include "ltfa/path_analysis.hpp"
#include <string>
namespace ltfa {
enum class ForwardingState { forwarding, blocking, unavailable, unknown };
enum class ForwardingReachability { reachable, unreachable, unknown };
[[nodiscard]] std::string_view to_string(ForwardingState) noexcept;
[[nodiscard]] std::string_view to_string(ForwardingReachability) noexcept;
struct ForwardingLinkResult {
    EntityId link;
    EndpointId first_port, second_port;
    EntityId first_device, second_device;
    // Effective segment availability combines cable, both ports and both devices.
    AvailabilityState physical{AvailabilityState::unknown};
    ForwardingState logical{ForwardingState::unknown};
    bool backup{false};
    AvailabilityState intrinsic_physical{AvailabilityState::unknown};
};
struct ForwardingTargetResult {
    EntityId target;
    ForwardingReachability reachability{ForwardingReachability::unknown};
    std::vector<EntityId> devices;
    std::vector<EntityId> links;
};
struct ForwardingResult {
    AnalysisStatus status{AnalysisStatus::invalid};
    EntityId domain;
    ObjectRef source;
    EntityId source_device;
    std::string protocol;
    std::vector<std::string> reasons;
    AssuranceQualification qualification;
    bool backup_activated{false};
    bool backup_activation_unknown{false};
    bool contains_unknown_availability{false};
    bool contains_degraded_availability{false};
    bool recovery_time_known{false};
    TraversalUsage usage;
    std::vector<ForwardingLinkResult> links;
    std::vector<ForwardingTargetResult> targets;
};
[[nodiscard]] ForwardingResult analyze_forwarding(const NormalizedModel &,
                                                  const AvailabilitySnapshot &, EntityId domain,
                                                  ObjectRef source,
                                                  const TraversalBudget &budget = {});
} // namespace ltfa
