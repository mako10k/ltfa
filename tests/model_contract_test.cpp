#include "ltfa/error.hpp"
#include "ltfa/limits.hpp"
#include "ltfa/model.hpp"

#include <array>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <type_traits>

namespace {

int failures = 0;

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << message << '\n';
        ++failures;
    }
}

template <typename Value, std::size_t Size>
void expect_names(const std::array<std::pair<Value, std::string_view>, Size> &cases) {
    for (const auto &[value, expected] : cases) {
        if (ltfa::to_string(value) != expected) {
            std::cerr << "unexpected enum name: expected " << expected << ", got "
                      << ltfa::to_string(value) << '\n';
            ++failures;
        }
    }
}

void test_ids_and_spans() {
    static_assert(!std::is_convertible_v<std::uint32_t, ltfa::EntityId>);
    static_assert(!std::is_same_v<ltfa::EntityId, ltfa::EndpointId>);

    const ltfa::EntityId entity{7};
    const ltfa::EndpointId endpoint{11};
    expect(entity.is_valid() && entity.value() == 7, "entity ID must retain its index");
    expect(!ltfa::EntityId{}.is_valid(), "default entity ID must be invalid");
    expect(ltfa::ObjectRef::from(entity).kind == ltfa::ObjectKind::entity,
           "entity reference must retain its kind");
    expect(ltfa::ObjectRef::from(endpoint).kind == ltfa::ObjectKind::endpoint,
           "endpoint reference must retain its kind");

    const ltfa::SourceSpan valid{
        ltfa::StringId{1},
        {0, 1, 1},
        {4, 1, 5},
    };
    const ltfa::SourceSpan reversed{
        ltfa::StringId{1},
        {4, 1, 5},
        {0, 1, 1},
    };
    expect(valid.is_valid(), "ordered source span must be valid");
    expect(!reversed.is_valid(), "reversed source span must be invalid");
}

void test_default_limits() {
    expect(ltfa::kDefaultModelLimits.max_source_file_bytes == 16U * 1024U * 1024U,
           "source file limit must match the reviewed design");
    expect(ltfa::kDefaultModelLimits.max_total_source_bytes == 64U * 1024U * 1024U,
           "total source limit must remain explicit and bounded");
    expect(ltfa::kDefaultModelLimits.max_line_bytes == 8U * 1024U,
           "line limit must match the reviewed design");
    expect(ltfa::kDefaultModelLimits.max_include_depth == 16,
           "include depth must match the reviewed design");
    expect(ltfa::kDefaultModelLimits.max_entities == 100'000,
           "entity limit must match the reviewed design");
    expect(ltfa::kDefaultModelLimits.max_relations == 500'000,
           "relation limit must match the reviewed design");
    expect(ltfa::kDefaultModelLimits.max_observation_line_bytes == 64U * 1024U,
           "observation line limit must match the reviewed design");
}

void test_model_enum_names() {
    expect_names(std::array{
        std::pair{ltfa::ScopeKind::site, std::string_view{"site"}},
        std::pair{ltfa::ScopeKind::cloud, std::string_view{"cloud"}},
        std::pair{ltfa::ScopeKind::shared, std::string_view{"shared"}},
        std::pair{ltfa::ScopeKind::other, std::string_view{"other"}},
    });
    expect_names(std::array{
        std::pair{ltfa::Completeness::verified, std::string_view{"verified"}},
        std::pair{ltfa::Completeness::partial, std::string_view{"partial"}},
        std::pair{ltfa::Completeness::unknown, std::string_view{"unknown"}},
    });
    expect_names(std::array{
        std::pair{ltfa::EvidenceQuality::confirmed, std::string_view{"confirmed"}},
        std::pair{ltfa::EvidenceQuality::inferred, std::string_view{"inferred"}},
        std::pair{ltfa::EvidenceQuality::unknown, std::string_view{"unknown"}},
    });
    expect_names(std::array{
        std::pair{ltfa::Sensitivity::internal, std::string_view{"internal"}},
        std::pair{ltfa::Sensitivity::restricted, std::string_view{"restricted"}},
    });
    expect_names(std::array{
        std::pair{ltfa::EntityKind::device, std::string_view{"device"}},
        std::pair{ltfa::EntityKind::facility, std::string_view{"facility"}},
        std::pair{ltfa::EntityKind::bus, std::string_view{"bus"}},
        std::pair{ltfa::EntityKind::network_space, std::string_view{"network-space"}},
        std::pair{ltfa::EntityKind::tunnel, std::string_view{"tunnel"}},
        std::pair{ltfa::EntityKind::service, std::string_view{"service"}},
        std::pair{ltfa::EntityKind::process, std::string_view{"process"}},
        std::pair{ltfa::EntityKind::other, std::string_view{"other"}},
    });
    expect_names(std::array{
        std::pair{ltfa::Plane::facility, std::string_view{"facility"}},
        std::pair{ltfa::Plane::communication, std::string_view{"communication"}},
        std::pair{ltfa::Plane::control_signal, std::string_view{"control-signal"}},
        std::pair{ltfa::Plane::service, std::string_view{"service"}},
    });
    expect_names(std::array{
        std::pair{ltfa::Layer::none, std::string_view{"none"}},
        std::pair{ltfa::Layer::physical, std::string_view{"physical"}},
        std::pair{ltfa::Layer::l2, std::string_view{"l2"}},
        std::pair{ltfa::Layer::l3, std::string_view{"l3"}},
        std::pair{ltfa::Layer::overlay, std::string_view{"overlay"}},
        std::pair{ltfa::Layer::virtual_network, std::string_view{"virtual-network"}},
        std::pair{ltfa::Layer::transport, std::string_view{"transport"}},
        std::pair{ltfa::Layer::application, std::string_view{"application"}},
    });
    expect_names(std::array{
        std::pair{ltfa::EndpointKind::network, std::string_view{"network"}},
        std::pair{ltfa::EndpointKind::power_input, std::string_view{"power-input"}},
        std::pair{ltfa::EndpointKind::power_output, std::string_view{"power-output"}},
        std::pair{ltfa::EndpointKind::serial, std::string_view{"serial"}},
        std::pair{ltfa::EndpointKind::contact_input, std::string_view{"contact-input"}},
        std::pair{ltfa::EndpointKind::contact_output, std::string_view{"contact-output"}},
        std::pair{ltfa::EndpointKind::analog_current_input,
                  std::string_view{"analog-current-input"}},
        std::pair{ltfa::EndpointKind::analog_current_output,
                  std::string_view{"analog-current-output"}},
        std::pair{ltfa::EndpointKind::logical, std::string_view{"logical"}},
    });
    expect_names(std::array{
        std::pair{ltfa::ObjectKind::entity, std::string_view{"entity"}},
        std::pair{ltfa::ObjectKind::endpoint, std::string_view{"endpoint"}},
    });
    expect_names(std::array{
        std::pair{ltfa::AvailabilityState::available, std::string_view{"available"}},
        std::pair{ltfa::AvailabilityState::degraded, std::string_view{"degraded"}},
        std::pair{ltfa::AvailabilityState::unavailable, std::string_view{"unavailable"}},
        std::pair{ltfa::AvailabilityState::unknown, std::string_view{"unknown"}},
    });
    expect_names(std::array{
        std::pair{ltfa::RelationKind::network, std::string_view{"network"}},
        std::pair{ltfa::RelationKind::power, std::string_view{"power"}},
        std::pair{ltfa::RelationKind::serial, std::string_view{"serial"}},
        std::pair{ltfa::RelationKind::contact, std::string_view{"contact"}},
        std::pair{ltfa::RelationKind::analog_current, std::string_view{"analog-current"}},
        std::pair{ltfa::RelationKind::member_of, std::string_view{"member_of"}},
        std::pair{ltfa::RelationKind::contained_in, std::string_view{"contained_in"}},
        std::pair{ltfa::RelationKind::carried_by, std::string_view{"carried_by"}},
        std::pair{ltfa::RelationKind::uses_underlay, std::string_view{"uses_underlay"}},
        std::pair{ltfa::RelationKind::terminates_on, std::string_view{"terminates_on"}},
        std::pair{ltfa::RelationKind::routes_via, std::string_view{"routes_via"}},
        std::pair{ltfa::RelationKind::depends_on, std::string_view{"depends_on"}},
        std::pair{ltfa::RelationKind::powered_by, std::string_view{"powered_by"}},
    });
    expect_names(std::array{
        std::pair{ltfa::Direction::directed, std::string_view{"directed"}},
        std::pair{ltfa::Direction::bidirectional, std::string_view{"bidirectional"}},
    });
    expect_names(std::array{
        std::pair{ltfa::RequirementMode::all, std::string_view{"all"}},
        std::pair{ltfa::RequirementMode::any, std::string_view{"any"}},
    });
    expect_names(std::array{
        std::pair{ltfa::FailureDomainKind::panel, std::string_view{"panel"}},
        std::pair{ltfa::FailureDomainKind::ups, std::string_view{"ups"}},
        std::pair{ltfa::FailureDomainKind::cabinet, std::string_view{"cabinet"}},
        std::pair{ltfa::FailureDomainKind::host, std::string_view{"host"}},
        std::pair{ltfa::FailureDomainKind::cable_route, std::string_view{"cable-route"}},
        std::pair{ltfa::FailureDomainKind::carrier, std::string_view{"carrier"}},
        std::pair{ltfa::FailureDomainKind::antenna, std::string_view{"antenna"}},
        std::pair{ltfa::FailureDomainKind::route_table, std::string_view{"route-table"}},
        std::pair{ltfa::FailureDomainKind::cloud_gateway, std::string_view{"cloud-gateway"}},
        std::pair{ltfa::FailureDomainKind::shared_service, std::string_view{"shared-service"}},
        std::pair{ltfa::FailureDomainKind::other, std::string_view{"other"}},
    });
    expect_names(std::array{
        std::pair{ltfa::ProbeKind::icmp, std::string_view{"icmp"}},
        std::pair{ltfa::ProbeKind::tcp_connect, std::string_view{"tcp-connect"}},
        std::pair{ltfa::ProbeKind::custom_heartbeat, std::string_view{"custom-heartbeat"}},
        std::pair{ltfa::ProbeKind::metric, std::string_view{"metric"}},
        std::pair{ltfa::ProbeKind::discrete_state, std::string_view{"discrete-state"}},
    });
}

void test_failure_mode_names() {
    expect_names(std::array{
        std::pair{ltfa::FailureModeKind::power_loss, std::string_view{"power-loss"}},
        std::pair{ltfa::FailureModeKind::breaker_trip, std::string_view{"breaker-trip"}},
        std::pair{ltfa::FailureModeKind::psu_loss, std::string_view{"psu-loss"}},
        std::pair{ltfa::FailureModeKind::undervoltage, std::string_view{"undervoltage"}},
        std::pair{ltfa::FailureModeKind::ups_depleted, std::string_view{"ups-depleted"}},
        std::pair{ltfa::FailureModeKind::link_down, std::string_view{"link-down"}},
        std::pair{ltfa::FailureModeKind::port_down, std::string_view{"port-down"}},
        std::pair{ltfa::FailureModeKind::device_failure, std::string_view{"device-failure"}},
        std::pair{ltfa::FailureModeKind::route_loss, std::string_view{"route-loss"}},
        std::pair{ltfa::FailureModeKind::bus_open, std::string_view{"bus-open"}},
        std::pair{ltfa::FailureModeKind::bus_short, std::string_view{"bus-short"}},
        std::pair{ltfa::FailureModeKind::termination_fault, std::string_view{"termination-fault"}},
        std::pair{ltfa::FailureModeKind::master_loss, std::string_view{"master-loss"}},
        std::pair{ltfa::FailureModeKind::noise, std::string_view{"noise"}},
        std::pair{ltfa::FailureModeKind::stuck_open, std::string_view{"stuck-open"}},
        std::pair{ltfa::FailureModeKind::stuck_closed, std::string_view{"stuck-closed"}},
        std::pair{ltfa::FailureModeKind::wetting_power_loss,
                  std::string_view{"wetting-power-loss"}},
        std::pair{ltfa::FailureModeKind::logic_inverted, std::string_view{"logic-inverted"}},
        std::pair{ltfa::FailureModeKind::loop_open, std::string_view{"loop-open"}},
        std::pair{ltfa::FailureModeKind::under_range, std::string_view{"under-range"}},
        std::pair{ltfa::FailureModeKind::over_range, std::string_view{"over-range"}},
        std::pair{ltfa::FailureModeKind::value_frozen, std::string_view{"value-frozen"}},
        std::pair{ltfa::FailureModeKind::sensor_power_loss, std::string_view{"sensor-power-loss"}},
        std::pair{ltfa::FailureModeKind::tunnel_down, std::string_view{"tunnel-down"}},
        std::pair{ltfa::FailureModeKind::peer_unreachable, std::string_view{"peer-unreachable"}},
        std::pair{ltfa::FailureModeKind::security_association_loss,
                  std::string_view{"security-association-loss"}},
        std::pair{ltfa::FailureModeKind::heartbeat_failed, std::string_view{"heartbeat-failed"}},
        std::pair{ltfa::FailureModeKind::heartbeat_stale, std::string_view{"heartbeat-stale"}},
        std::pair{ltfa::FailureModeKind::probe_path_loss, std::string_view{"probe-path-loss"}},
        std::pair{ltfa::FailureModeKind::extension, std::string_view{"extension"}},
    });
}

void test_diagnostic_codes() {
    expect_names(std::array{
        std::pair{ltfa::DiagnosticSeverity::error, std::string_view{"error"}},
        std::pair{ltfa::DiagnosticSeverity::warning, std::string_view{"warning"}},
    });
    expect_names(std::array{
        std::pair{ltfa::DiagnosticCode::dsl_invalid_header,
                  std::string_view{"E_DSL_INVALID_HEADER"}},
        std::pair{ltfa::DiagnosticCode::dsl_unexpected_token,
                  std::string_view{"E_DSL_UNEXPECTED_TOKEN"}},
        std::pair{ltfa::DiagnosticCode::dsl_invalid_utf8, std::string_view{"E_DSL_INVALID_UTF8"}},
        std::pair{ltfa::DiagnosticCode::dsl_bom_not_allowed,
                  std::string_view{"E_DSL_BOM_NOT_ALLOWED"}},
        std::pair{ltfa::DiagnosticCode::dsl_unterminated_string,
                  std::string_view{"E_DSL_UNTERMINATED_STRING"}},
        std::pair{ltfa::DiagnosticCode::dsl_invalid_character,
                  std::string_view{"E_DSL_INVALID_CHARACTER"}},
        std::pair{ltfa::DiagnosticCode::dsl_unknown_statement,
                  std::string_view{"E_DSL_UNKNOWN_STATEMENT"}},
        std::pair{ltfa::DiagnosticCode::dsl_invalid_attribute,
                  std::string_view{"E_DSL_INVALID_ATTRIBUTE"}},
        std::pair{ltfa::DiagnosticCode::dsl_duplicate_attribute,
                  std::string_view{"E_DSL_DUPLICATE_ATTRIBUTE"}},
        std::pair{ltfa::DiagnosticCode::dsl_invalid_value, std::string_view{"E_DSL_INVALID_VALUE"}},
        std::pair{ltfa::DiagnosticCode::dsl_missing_required_field,
                  std::string_view{"E_DSL_MISSING_REQUIRED_FIELD"}},
        std::pair{ltfa::DiagnosticCode::dsl_ambiguous_scope,
                  std::string_view{"E_DSL_AMBIGUOUS_SCOPE"}},
        std::pair{ltfa::DiagnosticCode::include_invalid, std::string_view{"E_INCLUDE_INVALID"}},
        std::pair{ltfa::DiagnosticCode::include_escape, std::string_view{"E_INCLUDE_ESCAPE"}},
        std::pair{ltfa::DiagnosticCode::include_cycle, std::string_view{"E_INCLUDE_CYCLE"}},
        std::pair{ltfa::DiagnosticCode::include_depth, std::string_view{"E_INCLUDE_DEPTH"}},
        std::pair{ltfa::DiagnosticCode::include_io, std::string_view{"E_INCLUDE_IO"}},
        std::pair{ltfa::DiagnosticCode::id_invalid, std::string_view{"E_ID_INVALID"}},
        std::pair{ltfa::DiagnosticCode::id_duplicate, std::string_view{"E_ID_DUPLICATE"}},
        std::pair{ltfa::DiagnosticCode::reference_unresolved,
                  std::string_view{"E_REFERENCE_UNRESOLVED"}},
        std::pair{ltfa::DiagnosticCode::endpoint_incompatible,
                  std::string_view{"E_ENDPOINT_INCOMPATIBLE"}},
        std::pair{ltfa::DiagnosticCode::layer_incompatible,
                  std::string_view{"E_LAYER_INCOMPATIBLE"}},
        std::pair{ltfa::DiagnosticCode::dependency_cycle, std::string_view{"E_DEPENDENCY_CYCLE"}},
        std::pair{ltfa::DiagnosticCode::limit_exceeded, std::string_view{"E_LIMIT_EXCEEDED"}},
        std::pair{ltfa::DiagnosticCode::limit_source_file_bytes,
                  std::string_view{"E_LIMIT_SOURCE_FILE_BYTES"}},
        std::pair{ltfa::DiagnosticCode::limit_total_source_bytes,
                  std::string_view{"E_LIMIT_TOTAL_SOURCE_BYTES"}},
        std::pair{ltfa::DiagnosticCode::limit_line_bytes, std::string_view{"E_LIMIT_LINE_BYTES"}},
        std::pair{ltfa::DiagnosticCode::observation_invalid,
                  std::string_view{"E_OBSERVATION_INVALID"}},
    });
}

} // namespace

int main() {
    test_ids_and_spans();
    test_default_limits();
    test_model_enum_names();
    test_failure_mode_names();
    test_diagnostic_codes();
    return failures == 0 ? 0 : 1;
}
