#include "ltfa/model.hpp"

namespace ltfa {

std::string_view to_string(ScopeKind value) noexcept {
    switch (value) {
    case ScopeKind::site:
        return "site";
    case ScopeKind::cloud:
        return "cloud";
    case ScopeKind::shared:
        return "shared";
    case ScopeKind::other:
        return "other";
    }
    return "unknown";
}

std::string_view to_string(Completeness value) noexcept {
    switch (value) {
    case Completeness::verified:
        return "verified";
    case Completeness::partial:
        return "partial";
    case Completeness::unknown:
        return "unknown";
    }
    return "unknown";
}

std::string_view to_string(EvidenceQuality value) noexcept {
    switch (value) {
    case EvidenceQuality::confirmed:
        return "confirmed";
    case EvidenceQuality::inferred:
        return "inferred";
    case EvidenceQuality::unknown:
        return "unknown";
    }
    return "unknown";
}

std::string_view to_string(Sensitivity value) noexcept {
    switch (value) {
    case Sensitivity::internal:
        return "internal";
    case Sensitivity::restricted:
        return "restricted";
    }
    return "unknown";
}

std::string_view to_string(EntityKind value) noexcept {
    switch (value) {
    case EntityKind::device:
        return "device";
    case EntityKind::facility:
        return "facility";
    case EntityKind::bus:
        return "bus";
    case EntityKind::network_space:
        return "network-space";
    case EntityKind::tunnel:
        return "tunnel";
    case EntityKind::service:
        return "service";
    case EntityKind::process:
        return "process";
    case EntityKind::other:
        return "other";
    }
    return "unknown";
}

std::string_view to_string(Plane value) noexcept {
    switch (value) {
    case Plane::facility:
        return "facility";
    case Plane::communication:
        return "communication";
    case Plane::control_signal:
        return "control-signal";
    case Plane::service:
        return "service";
    }
    return "unknown";
}

std::string_view to_string(Layer value) noexcept {
    switch (value) {
    case Layer::none:
        return "none";
    case Layer::physical:
        return "physical";
    case Layer::l2:
        return "l2";
    case Layer::l3:
        return "l3";
    case Layer::overlay:
        return "overlay";
    case Layer::virtual_network:
        return "virtual-network";
    case Layer::transport:
        return "transport";
    case Layer::application:
        return "application";
    }
    return "unknown";
}

std::string_view to_string(EndpointKind value) noexcept {
    switch (value) {
    case EndpointKind::network:
        return "network";
    case EndpointKind::power_input:
        return "power-input";
    case EndpointKind::power_output:
        return "power-output";
    case EndpointKind::serial:
        return "serial";
    case EndpointKind::contact_input:
        return "contact-input";
    case EndpointKind::contact_output:
        return "contact-output";
    case EndpointKind::analog_current_input:
        return "analog-current-input";
    case EndpointKind::analog_current_output:
        return "analog-current-output";
    case EndpointKind::logical:
        return "logical";
    }
    return "unknown";
}

std::string_view to_string(ObjectKind value) noexcept {
    switch (value) {
    case ObjectKind::entity:
        return "entity";
    case ObjectKind::endpoint:
        return "endpoint";
    }
    return "unknown";
}

std::string_view to_string(RelationKind value) noexcept {
    switch (value) {
    case RelationKind::network:
        return "network";
    case RelationKind::power:
        return "power";
    case RelationKind::serial:
        return "serial";
    case RelationKind::contact:
        return "contact";
    case RelationKind::analog_current:
        return "analog-current";
    case RelationKind::member_of:
        return "member_of";
    case RelationKind::contained_in:
        return "contained_in";
    case RelationKind::carried_by:
        return "carried_by";
    case RelationKind::uses_underlay:
        return "uses_underlay";
    case RelationKind::terminates_on:
        return "terminates_on";
    case RelationKind::routes_via:
        return "routes_via";
    case RelationKind::depends_on:
        return "depends_on";
    case RelationKind::powered_by:
        return "powered_by";
    }
    return "unknown";
}

std::string_view to_string(Direction value) noexcept {
    switch (value) {
    case Direction::directed:
        return "directed";
    case Direction::bidirectional:
        return "bidirectional";
    }
    return "unknown";
}

std::string_view to_string(RequirementMode value) noexcept {
    switch (value) {
    case RequirementMode::all:
        return "all";
    case RequirementMode::any:
        return "any";
    }
    return "unknown";
}

std::string_view to_string(AvailabilityState value) noexcept {
    switch (value) {
    case AvailabilityState::available:
        return "available";
    case AvailabilityState::degraded:
        return "degraded";
    case AvailabilityState::unavailable:
        return "unavailable";
    case AvailabilityState::unknown:
        return "unknown";
    }
    return "unknown";
}

std::string_view to_string(FailureDomainKind value) noexcept {
    switch (value) {
    case FailureDomainKind::panel:
        return "panel";
    case FailureDomainKind::ups:
        return "ups";
    case FailureDomainKind::cabinet:
        return "cabinet";
    case FailureDomainKind::host:
        return "host";
    case FailureDomainKind::cable_route:
        return "cable-route";
    case FailureDomainKind::carrier:
        return "carrier";
    case FailureDomainKind::antenna:
        return "antenna";
    case FailureDomainKind::route_table:
        return "route-table";
    case FailureDomainKind::cloud_gateway:
        return "cloud-gateway";
    case FailureDomainKind::shared_service:
        return "shared-service";
    case FailureDomainKind::other:
        return "other";
    }
    return "unknown";
}

std::string_view to_string(FailureModeKind value) noexcept {
    switch (value) {
    case FailureModeKind::power_loss:
        return "power-loss";
    case FailureModeKind::breaker_trip:
        return "breaker-trip";
    case FailureModeKind::psu_loss:
        return "psu-loss";
    case FailureModeKind::undervoltage:
        return "undervoltage";
    case FailureModeKind::ups_depleted:
        return "ups-depleted";
    case FailureModeKind::link_down:
        return "link-down";
    case FailureModeKind::port_down:
        return "port-down";
    case FailureModeKind::device_failure:
        return "device-failure";
    case FailureModeKind::route_loss:
        return "route-loss";
    case FailureModeKind::bus_open:
        return "bus-open";
    case FailureModeKind::bus_short:
        return "bus-short";
    case FailureModeKind::termination_fault:
        return "termination-fault";
    case FailureModeKind::master_loss:
        return "master-loss";
    case FailureModeKind::noise:
        return "noise";
    case FailureModeKind::stuck_open:
        return "stuck-open";
    case FailureModeKind::stuck_closed:
        return "stuck-closed";
    case FailureModeKind::wetting_power_loss:
        return "wetting-power-loss";
    case FailureModeKind::logic_inverted:
        return "logic-inverted";
    case FailureModeKind::loop_open:
        return "loop-open";
    case FailureModeKind::under_range:
        return "under-range";
    case FailureModeKind::over_range:
        return "over-range";
    case FailureModeKind::value_frozen:
        return "value-frozen";
    case FailureModeKind::sensor_power_loss:
        return "sensor-power-loss";
    case FailureModeKind::tunnel_down:
        return "tunnel-down";
    case FailureModeKind::peer_unreachable:
        return "peer-unreachable";
    case FailureModeKind::security_association_loss:
        return "security-association-loss";
    case FailureModeKind::heartbeat_failed:
        return "heartbeat-failed";
    case FailureModeKind::heartbeat_stale:
        return "heartbeat-stale";
    case FailureModeKind::probe_path_loss:
        return "probe-path-loss";
    case FailureModeKind::extension:
        return "extension";
    }
    return "unknown";
}

std::string_view to_string(ProbeKind value) noexcept {
    switch (value) {
    case ProbeKind::icmp:
        return "icmp";
    case ProbeKind::tcp_connect:
        return "tcp-connect";
    case ProbeKind::custom_heartbeat:
        return "custom-heartbeat";
    case ProbeKind::metric:
        return "metric";
    case ProbeKind::discrete_state:
        return "discrete-state";
    }
    return "unknown";
}

std::string_view to_string(AttributeValueKind value) noexcept {
    switch (value) {
    case AttributeValueKind::identifier:
        return "identifier";
    case AttributeValueKind::string:
        return "string";
    case AttributeValueKind::integer:
        return "integer";
    case AttributeValueKind::real:
        return "real";
    case AttributeValueKind::boolean:
        return "boolean";
    case AttributeValueKind::duration:
        return "duration";
    }
    return "unknown";
}

std::string_view NormalizedModel::string(StringId id) const noexcept {
    if (!id.is_valid() || id.value() >= strings.size()) {
        return {};
    }
    return strings[id.value()];
}

std::string_view NormalizedModel::external_id(ObjectRef object) const noexcept {
    if (!object.is_valid()) {
        return {};
    }
    if (object.kind == ObjectKind::entity) {
        if (object.index >= entities.size()) {
            return {};
        }
        return string(entities[object.index].external_id);
    }
    if (object.index >= endpoints.size()) {
        return {};
    }
    return string(endpoints[object.index].external_id);
}

} // namespace ltfa
