#pragma once

#include "ltfa/ids.hpp"
#include "ltfa/source.hpp"

#include <cstdint>
#include <string_view>

namespace ltfa {

enum class ScopeKind {
    site,
    cloud,
    shared,
    other,
};

enum class Completeness {
    verified,
    partial,
    unknown,
};

enum class EvidenceQuality {
    confirmed,
    inferred,
    unknown,
};

enum class Sensitivity {
    internal,
    restricted,
};

enum class EntityKind {
    device,
    facility,
    bus,
    network_space,
    tunnel,
    service,
    process,
    other,
};

enum class Plane {
    facility,
    communication,
    control_signal,
    service,
};

enum class Layer {
    none,
    physical,
    l2,
    l3,
    overlay,
    virtual_network,
    transport,
    application,
};

enum class EndpointKind {
    network,
    power_input,
    power_output,
    serial,
    contact_input,
    contact_output,
    analog_current_input,
    analog_current_output,
    logical,
};

enum class ObjectKind {
    entity,
    endpoint,
};

enum class RelationKind {
    network,
    power,
    serial,
    contact,
    analog_current,
    member_of,
    contained_in,
    carried_by,
    uses_underlay,
    terminates_on,
    routes_via,
    depends_on,
    powered_by,
};

enum class Direction {
    directed,
    bidirectional,
};

enum class RequirementMode {
    all,
    any,
};

enum class AvailabilityState {
    available,
    degraded,
    unavailable,
    unknown,
};

enum class FailureDomainKind {
    panel,
    ups,
    cabinet,
    host,
    cable_route,
    carrier,
    antenna,
    route_table,
    cloud_gateway,
    shared_service,
    other,
};

enum class FailureModeKind {
    power_loss,
    breaker_trip,
    psu_loss,
    undervoltage,
    ups_depleted,
    link_down,
    port_down,
    device_failure,
    route_loss,
    bus_open,
    bus_short,
    termination_fault,
    master_loss,
    noise,
    stuck_open,
    stuck_closed,
    wetting_power_loss,
    logic_inverted,
    loop_open,
    under_range,
    over_range,
    value_frozen,
    sensor_power_loss,
    tunnel_down,
    peer_unreachable,
    security_association_loss,
    heartbeat_failed,
    heartbeat_stale,
    probe_path_loss,
    extension,
};

enum class ProbeKind {
    icmp,
    tcp_connect,
    custom_heartbeat,
    metric,
    discrete_state,
};

[[nodiscard]] std::string_view to_string(ScopeKind value) noexcept;
[[nodiscard]] std::string_view to_string(Completeness value) noexcept;
[[nodiscard]] std::string_view to_string(EvidenceQuality value) noexcept;
[[nodiscard]] std::string_view to_string(Sensitivity value) noexcept;
[[nodiscard]] std::string_view to_string(EntityKind value) noexcept;
[[nodiscard]] std::string_view to_string(Plane value) noexcept;
[[nodiscard]] std::string_view to_string(Layer value) noexcept;
[[nodiscard]] std::string_view to_string(EndpointKind value) noexcept;
[[nodiscard]] std::string_view to_string(ObjectKind value) noexcept;
[[nodiscard]] std::string_view to_string(RelationKind value) noexcept;
[[nodiscard]] std::string_view to_string(Direction value) noexcept;
[[nodiscard]] std::string_view to_string(RequirementMode value) noexcept;
[[nodiscard]] std::string_view to_string(AvailabilityState value) noexcept;
[[nodiscard]] std::string_view to_string(FailureDomainKind value) noexcept;
[[nodiscard]] std::string_view to_string(FailureModeKind value) noexcept;
[[nodiscard]] std::string_view to_string(ProbeKind value) noexcept;

struct ObjectRef {
    ObjectKind kind{ObjectKind::entity};
    std::uint32_t index{EntityId::invalid_value()};

    [[nodiscard]] static constexpr ObjectRef from(EntityId id) noexcept {
        return {ObjectKind::entity, id.value()};
    }

    [[nodiscard]] static constexpr ObjectRef from(EndpointId id) noexcept {
        return {ObjectKind::endpoint, id.value()};
    }

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return index != EntityId::invalid_value();
    }
};

struct Scope {
    ScopeId id;
    StringId external_id;
    StringId label;
    ScopeKind kind{ScopeKind::other};
    Completeness completeness{Completeness::unknown};
    EvidenceQuality evidence_quality{EvidenceQuality::unknown};
    SourceSpan source;
};

struct Entity {
    EntityId id;
    StringId external_id;
    StringId label;
    EntityKind kind{EntityKind::other};
    Plane plane{Plane::communication};
    Layer layer{Layer::none};
    ScopeId scope;
    StringId network_space;
    IndexRange failure_domains;
    IndexRange attributes;
    Sensitivity sensitivity{Sensitivity::internal};
    StringId source_ref;
    SourceSpan source;
};

struct Endpoint {
    EndpointId id;
    StringId external_id;
    EntityId owner;
    EndpointKind kind{EndpointKind::logical};
    IndexRange attributes;
    SourceSpan source;
};

struct Relation {
    RelationId id;
    StringId external_id;
    ObjectRef from;
    ObjectRef to;
    RelationKind kind{RelationKind::depends_on};
    Direction direction{Direction::directed};
    IndexRange attributes;
    SourceSpan source;
};

struct RequirementGroup {
    RequirementGroupId id;
    StringId external_id;
    EntityId consumer;
    RequirementMode mode{RequirementMode::all};
    IndexRange providers;
    std::uint64_t hold_up_ms{0};
    SourceSpan source;
};

struct FailureDomain {
    FailureDomainId id;
    StringId external_id;
    FailureDomainKind kind{FailureDomainKind::other};
    IndexRange members;
    SourceSpan source;
};

struct FailureMode {
    FailureModeId id;
    StringId external_id;
    ObjectRef target;
    FailureModeKind kind{FailureModeKind::extension};
    std::uint64_t delay_ms{0};
    SourceSpan source;
};

struct Probe {
    ProbeId id;
    StringId external_id;
    ObjectRef target;
    ObjectRef vantage;
    ProbeKind kind{ProbeKind::icmp};
    std::uint64_t valid_for_ms{0};
    SourceSpan source;
};

} // namespace ltfa
