// R: Verify propagation semantics, simultaneous-state behavior and single-injection compatibility.
#include "ltfa/dsl/parser.hpp"
#include "ltfa/failure_propagation.hpp"
#include "ltfa/model_compiler.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr ltfa::StringId kFile{11};

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

ltfa::NormalizedModel compile_fixture() {
    const std::string source =
        "ltfa 1\n"
        "scope site:verified kind=site completeness=verified "
        "evidence-quality=confirmed\n"
        "scope site:partial kind=site completeness=partial evidence-quality=inferred\n"
        "entity facility:breaker kind=facility plane=facility layer=physical "
        "scope=site:verified failure-domain=shared-power\n"
        "entity facility:backup kind=facility plane=facility layer=physical "
        "scope=site:verified failure-domain=shared-power\n"
        "entity service:immediate kind=service plane=service layer=application "
        "scope=site:verified\n"
        "entity service:held kind=service plane=service layer=application "
        "scope=site:verified\n"
        "entity service:held-child kind=service plane=service layer=application "
        "scope=site:verified\n"
        "entity service:held-peer kind=service plane=service layer=application "
        "scope=site:verified\n"
        "entity service:redundant kind=service plane=service layer=application "
        "scope=site:verified\n"
        "entity facility:voltage kind=facility plane=facility layer=physical "
        "scope=site:verified\n"
        "entity service:voltage kind=service plane=service layer=application "
        "scope=site:verified\n"
        "entity service:mystery kind=service plane=service layer=application "
        "scope=site:partial\n"
        "entity service:unknown kind=service plane=service layer=application "
        "scope=site:partial\n"
        "entity service:relation-provider kind=service plane=service layer=application "
        "scope=site:verified\n"
        "entity service:relation-consumer kind=service plane=service layer=application "
        "scope=site:verified\n"
        "entity process:observer kind=process plane=service layer=application "
        "scope=site:verified\n"
        "entity facility:panel kind=facility plane=facility layer=physical "
        "scope=site:verified\n"
        "entity device:load kind=device plane=communication layer=physical "
        "scope=site:verified\n"
        "entity device:peer kind=device plane=communication layer=physical "
        "scope=site:verified\n"
        "endpoint endpoint:panel:out owner=facility:panel kind=power-output\n"
        "endpoint endpoint:load:power owner=device:load kind=power-input\n"
        "endpoint endpoint:peer:net owner=device:peer kind=network\n"
        "endpoint endpoint:load:net owner=device:load kind=network\n"
        "require service:immediate by facility:breaker group=power mode=all\n"
        "require service:held by facility:breaker group=power mode=all hold-up=30s\n"
        "require service:held-peer by facility:breaker group=power mode=all hold-up=30s\n"
        "require service:redundant by facility:breaker group=power mode=any\n"
        "require service:redundant by facility:backup group=power mode=any\n"
        "require service:voltage by facility:voltage group=power mode=all\n"
        "require service:unknown by service:mystery group=runtime mode=all\n"
        "relate service:held-child -> service:held kind=depends_on\n"
        "relate service:relation-consumer -> service:relation-provider kind=depends_on\n"
        "connect endpoint:panel:out -> endpoint:load:power kind=power\n"
        "connect endpoint:peer:net -- endpoint:load:net kind=network\n"
        "failure failure:breaker target=facility:breaker mode=breaker-trip\n"
        "failure failure:voltage target=facility:voltage mode=undervoltage after=5s\n"
        "failure failure:custom target=service:mystery mode=x-unresolved\n"
        "probe probe:breaker target=facility:breaker vantage=process:observer kind=metric "
        "valid-for=60s\n"
        "probe probe:held target=service:held vantage=process:observer kind=metric "
        "valid-for=60s\n";

    auto parsed = ltfa::dsl::parse_document(source, kFile);
    expect(parsed.ok(), "propagation fixture must parse");
    ltfa::dsl::LoadedDocument loaded;
    loaded.statements = std::move(parsed.document->statements);
    loaded.total_source_bytes = source.size();
    auto compiled = ltfa::compile_model(loaded);
    if (compiled.error.has_value()) {
        std::cerr << ltfa::to_string(compiled.error->code) << ": " << compiled.error->message
                  << '\n';
    }
    expect(compiled.ok(), "propagation fixture must compile");
    return std::move(*compiled.model);
}

ltfa::ObjectRef find_object(const ltfa::NormalizedModel &model, std::string_view external_id) {
    for (const auto &entity : model.entities) {
        if (model.string(entity.external_id) == external_id) {
            return ltfa::ObjectRef::from(entity.id);
        }
    }
    for (const auto &endpoint : model.endpoints) {
        if (model.string(endpoint.external_id) == external_id) {
            return ltfa::ObjectRef::from(endpoint.id);
        }
    }
    return {};
}

ltfa::FailureModeId find_failure(const ltfa::NormalizedModel &model, std::string_view external_id) {
    for (const auto &failure : model.failure_modes) {
        if (model.string(failure.external_id) == external_id) {
            return failure.id;
        }
    }
    return {};
}

ltfa::FailureDomainId find_domain(const ltfa::NormalizedModel &model,
                                  std::string_view external_id) {
    for (const auto &domain : model.failure_domains) {
        if (model.string(domain.external_id) == external_id) {
            return domain.id;
        }
    }
    return {};
}

ltfa::RelationId find_relation(const ltfa::NormalizedModel &model, std::string_view from,
                               std::string_view to, ltfa::RelationKind kind) {
    for (const auto &relation : model.relations) {
        if (model.external_id(relation.from) == from && model.external_id(relation.to) == to &&
            relation.kind == kind) {
            return relation.id;
        }
    }
    return {};
}

std::vector<std::string_view> object_ids(const ltfa::NormalizedModel &model,
                                         const std::vector<ltfa::ObjectRef> &objects) {
    std::vector<std::string_view> result;
    for (const auto object : objects) {
        result.push_back(model.external_id(object));
    }
    return result;
}

std::vector<std::string> event_facts(const ltfa::NormalizedModel &model,
                                     const std::vector<ltfa::PropagationEvent> &events) {
    std::vector<std::string> result;
    for (const auto &event : events) {
        result.push_back(std::to_string(event.effective_time_ms) + ":" +
                         std::string{model.external_id(event.object)} + ":" +
                         std::string{ltfa::to_string(event.state)});
    }
    return result;
}

void test_failure_mode_table() {
    using Kind = ltfa::FailureModeKind;
    using State = ltfa::AvailabilityState;
    constexpr std::array degraded{
        Kind::undervoltage, Kind::termination_fault, Kind::noise,        Kind::logic_inverted,
        Kind::under_range,  Kind::over_range,        Kind::value_frozen, Kind::heartbeat_stale,
    };
    constexpr std::array unavailable{
        Kind::power_loss,
        Kind::breaker_trip,
        Kind::psu_loss,
        Kind::ups_depleted,
        Kind::link_down,
        Kind::port_down,
        Kind::device_failure,
        Kind::route_loss,
        Kind::bus_open,
        Kind::bus_short,
        Kind::master_loss,
        Kind::stuck_open,
        Kind::stuck_closed,
        Kind::wetting_power_loss,
        Kind::loop_open,
        Kind::sensor_power_loss,
        Kind::tunnel_down,
        Kind::peer_unreachable,
        Kind::security_association_loss,
        Kind::heartbeat_failed,
        Kind::probe_path_loss,
    };
    for (const auto mode : degraded) {
        expect(ltfa::failure_mode_result(mode) == State::degraded,
               "signal-quality failure modes must deterministically degrade");
    }
    for (const auto mode : unavailable) {
        expect(ltfa::failure_mode_result(mode) == State::unavailable,
               "hard-loss failure modes must deterministically become unavailable");
    }
    expect(ltfa::failure_mode_result(Kind::extension) == State::unknown,
           "extension failure modes must retain unresolved semantics");
}

void test_breaker_and_hold_up(const ltfa::NormalizedModel &model) {
    const auto initial = ltfa::make_availability_snapshot(model);
    const auto breaker = find_failure(model, "failure:breaker");
    const auto result =
        ltfa::propagate_failure(model, initial, ltfa::FailureInjection::for_declared_mode(breaker));

    expect(result.status == ltfa::PropagationStatus::complete &&
               event_facts(model, result.events) ==
                   std::vector<std::string>{
                       "0:facility:breaker:unavailable", "0:service:immediate:unavailable",
                       "30000:service:held:unavailable", "30000:service:held-child:unavailable",
                       "30000:service:held-peer:unavailable"},
           "breaker propagation must retain immediate and stable delayed transitions");
    expect(object_ids(model, result.immediate_unavailable) ==
                   std::vector<std::string_view>{"facility:breaker", "service:immediate"} &&
               object_ids(model, result.delayed_unavailable) ==
                   std::vector<std::string_view>{"service:held", "service:held-child",
                                                 "service:held-peer"},
           "breaker classifications must distinguish immediate and delayed loss");
    expect(result.final_snapshot.state(find_object(model, "service:redundant")) ==
               ltfa::AvailabilityState::available,
           "any-provider redundancy must retain the consumer");
    expect(result.lost_probes.size() == 2 &&
               object_ids(model, result.detection_gaps) ==
                   std::vector<std::string_view>{"service:held-child", "service:held-peer",
                                                 "service:immediate"},
           "lost probes and unobserved affected objects must remain explicit");
    expect(std::find(result.unaffected_objects.begin(), result.unaffected_objects.end(),
                     find_object(model, "service:redundant")) != result.unaffected_objects.end(),
           "retained redundant consumers must be classified as unaffected");

    const auto repeated =
        ltfa::propagate_failure(model, initial, ltfa::FailureInjection::for_declared_mode(breaker));
    expect(event_facts(model, repeated.events) == event_facts(model, result.events) &&
               repeated.usage.scheduled_events == result.usage.scheduled_events &&
               repeated.usage.processed_events == result.usage.processed_events &&
               repeated.usage.state_transitions == result.usage.state_transitions,
           "identical propagation must retain byte-stable event facts and usage");
}

void test_degraded_and_unknown_modes(const ltfa::NormalizedModel &model) {
    const auto initial = ltfa::make_availability_snapshot(model);
    const auto voltage_object = find_object(model, "facility:voltage");
    const auto immediate = ltfa::propagate_failure(
        model, initial,
        ltfa::FailureInjection::for_object(voltage_object, ltfa::AvailabilityState::degraded));
    expect(object_ids(model, immediate.immediate_degraded) ==
               std::vector<std::string_view>{"facility:voltage", "service:voltage"},
           "direct degraded injection must classify immediate degraded operation");

    const auto voltage = find_failure(model, "failure:voltage");
    const auto degraded =
        ltfa::propagate_failure(model, initial, ltfa::FailureInjection::for_declared_mode(voltage));
    expect(degraded.status == ltfa::PropagationStatus::complete &&
               object_ids(model, degraded.delayed_degraded) ==
                   std::vector<std::string_view>{"facility:voltage", "service:voltage"} &&
               event_facts(model, degraded.events) ==
                   std::vector<std::string>{"5000:facility:voltage:degraded",
                                            "5000:service:voltage:degraded"},
           "declared mode delay must propagate degraded operation at a fixed time");

    const auto custom = find_failure(model, "failure:custom");
    const auto unresolved =
        ltfa::propagate_failure(model, initial, ltfa::FailureInjection::for_declared_mode(custom));
    expect(unresolved.status == ltfa::PropagationStatus::complete &&
               unresolved.unresolved_failure_mode &&
               object_ids(model, unresolved.unknown_objects) ==
                   std::vector<std::string_view>{"service:mystery", "service:unknown"} &&
               unresolved.qualification.completeness == ltfa::Completeness::partial &&
               unresolved.qualification.evidence_quality == ltfa::EvidenceQuality::inferred,
           "extension modes must propagate unknown without inventing semantics");
}

void test_domain_endpoint_and_relation_injection(const ltfa::NormalizedModel &model) {
    const auto initial = ltfa::make_availability_snapshot(model);
    const auto shared = find_domain(model, "shared-power");
    const auto domain =
        ltfa::propagate_failure(model, initial, ltfa::FailureInjection::for_failure_domain(shared));
    expect(domain.status == ltfa::PropagationStatus::complete &&
               domain.final_snapshot.state(find_object(model, "service:redundant")) ==
                   ltfa::AvailabilityState::unavailable &&
               object_ids(model, domain.immediate_unavailable) ==
                   std::vector<std::string_view>{"facility:backup", "facility:breaker",
                                                 "service:immediate", "service:redundant"},
           "domain failure must atomically remove every redundant provider before propagation");

    const auto power_source = find_object(model, "endpoint:panel:out");
    const auto power_target = find_object(model, "endpoint:load:power");
    const auto endpoint =
        ltfa::propagate_failure(model, initial, ltfa::FailureInjection::for_object(power_source));
    expect(object_ids(model, endpoint.immediate_unavailable) ==
                   std::vector<std::string_view>{"endpoint:load:power", "endpoint:panel:out"} &&
               endpoint.final_snapshot.state(power_target) == ltfa::AvailabilityState::unavailable,
           "directed power dependencies must propagate endpoint failure");

    const auto power_relation = find_relation(model, "endpoint:panel:out", "endpoint:load:power",
                                              ltfa::RelationKind::power);
    const auto failed_power = ltfa::propagate_failure(
        model, initial, ltfa::FailureInjection::for_relation(power_relation));
    expect(failed_power.failed_relations == std::vector<ltfa::RelationId>{power_relation} &&
               object_ids(model, failed_power.immediate_unavailable) ==
                   std::vector<std::string_view>{"endpoint:load:power"},
           "availability-relation failure must affect only its consumer");

    const auto network_relation =
        find_relation(model, "endpoint:load:net", "endpoint:peer:net", ltfa::RelationKind::network);
    const auto failed_network = ltfa::propagate_failure(
        model, initial, ltfa::FailureInjection::for_relation(network_relation));
    expect(failed_network.status == ltfa::PropagationStatus::complete &&
               failed_network.events.empty() &&
               failed_network.failed_relations == std::vector<ltfa::RelationId>{network_relation},
           "connectivity-only relation failure must not invent availability propagation");
}

void test_budget_and_invalid_input(const ltfa::NormalizedModel &model) {
    const auto initial = ltfa::make_availability_snapshot(model);
    const auto breaker = find_failure(model, "failure:breaker");

    ltfa::PropagationBudget one_event;
    one_event.max_events = 1;
    const auto event_limited = ltfa::propagate_failure(
        model, initial, ltfa::FailureInjection::for_declared_mode(breaker), one_event);
    expect(event_limited.status == ltfa::PropagationStatus::incomplete &&
               event_limited.usage.scheduled_events == 1,
           "event budget exhaustion must return an explicit incomplete result");

    ltfa::PropagationBudget one_transition;
    one_transition.max_state_transitions = 1;
    const auto transition_limited = ltfa::propagate_failure(
        model, initial, ltfa::FailureInjection::for_declared_mode(breaker), one_transition);
    expect(transition_limited.status == ltfa::PropagationStatus::incomplete &&
               transition_limited.usage.state_transitions == 1,
           "state-transition budget exhaustion must retain the reached bound");

    expect(ltfa::propagate_failure(
               model, initial, ltfa::FailureInjection::for_declared_mode(ltfa::FailureModeId{99}))
                   .status == ltfa::PropagationStatus::invalid,
           "invalid declared failure IDs must fail closed");
    expect(ltfa::propagate_failure(
               model, initial,
               ltfa::FailureInjection::for_object(find_object(model, "facility:breaker"),
                                                  ltfa::AvailabilityState::available))
                   .status == ltfa::PropagationStatus::invalid,
           "an available-state injection must not be accepted as a failure");

    ltfa::AvailabilitySnapshot undersized;
    expect(ltfa::propagate_failure(model, undersized,
                                   ltfa::FailureInjection::for_declared_mode(breaker))
                   .status == ltfa::PropagationStatus::invalid,
           "undersized initial snapshots must fail closed");

    auto malformed = model;
    malformed.availability_adjacency_entries.front().object = {};
    expect(ltfa::propagate_failure(malformed, initial,
                                   ltfa::FailureInjection::for_declared_mode(breaker))
                   .status == ltfa::PropagationStatus::invalid,
           "malformed normalized adjacency must fail closed");

    malformed = model;
    malformed.failure_modes[breaker.value()].kind = static_cast<ltfa::FailureModeKind>(99);
    expect(ltfa::propagate_failure(malformed, initial,
                                   ltfa::FailureInjection::for_declared_mode(breaker))
                   .status == ltfa::PropagationStatus::invalid,
           "unknown built-in failure-mode enum values must fail closed");

    malformed = model;
    const auto held = find_object(malformed, "service:held");
    for (auto &group : malformed.requirement_groups) {
        if (group.consumer == ltfa::EntityId{held.index}) {
            group.hold_up_ms = std::numeric_limits<std::uint64_t>::max();
        }
    }
    expect(ltfa::propagate_failure(
               malformed, initial,
               ltfa::FailureInjection::for_object(find_object(malformed, "facility:breaker"),
                                                  ltfa::AvailabilityState::unavailable, 1))
                   .status == ltfa::PropagationStatus::invalid,
           "overflowing fixed-delay timestamps must fail closed");
}

void test_simultaneous_states(const ltfa::NormalizedModel &model) {
    using S = ltfa::AvailabilityState;
    auto initial = ltfa::make_availability_snapshot(model);
    auto breaker = find_object(model, "facility:breaker");
    auto backup = find_object(model, "facility:backup");
    auto redundant = find_object(model, "service:redundant");
    auto a = ltfa::FailureInjection::for_object(breaker, S::unavailable);
    auto b = ltfa::FailureInjection::for_object(backup, S::unavailable);
    auto normal = ltfa::FailureInjection::for_object(redundant, S::available);
    auto combined = ltfa::propagate_states(model, initial, {a, b, normal});
    auto reversed = ltfa::propagate_states(model, initial, {normal, b, a});
    expect(combined.status == ltfa::PropagationStatus::complete, "batch must complete");
    expect(combined.final_snapshot.state(redundant) == S::unavailable,
           "two failed any providers must fail consumer despite consumer available input");
    expect(combined.final_snapshot.entity_states == reversed.final_snapshot.entity_states &&
               event_facts(model, combined.events) == event_facts(model, reversed.events),
           "batch must be order independent");
    expect(ltfa::propagate_failure(model, initial, a).final_snapshot.state(redundant) ==
               S::available,
           "single failed any provider must preserve redundant consumer");
    auto degraded = ltfa::propagate_states(
        model, initial, {ltfa::FailureInjection::for_object(breaker, S::degraded)});
    expect(degraded.final_snapshot.state(find_object(model, "service:immediate")) == S::degraded,
           "degraded must propagate");
    auto unknown = ltfa::propagate_states(
        model, initial, {ltfa::FailureInjection::for_object(breaker, S::unknown)});
    expect(unknown.final_snapshot.state(find_object(model, "service:immediate")) == S::unknown,
           "unknown must propagate");
    expect(ltfa::propagate_states(model, initial, {a, a}).status ==
               ltfa::PropagationStatus::invalid,
           "duplicate objects rejected");
    expect(ltfa::propagate_states(model, initial, {}).status == ltfa::PropagationStatus::complete,
           "empty input is nominal");
    ltfa::PropagationBudget budget;
    budget.max_events = 1;
    expect(ltfa::propagate_states(model, initial, {a, b}, budget).status ==
               ltfa::PropagationStatus::incomplete,
           "batch event budget enforced");
    expect(ltfa::propagate_failure(model, initial, normal).status ==
               ltfa::PropagationStatus::invalid,
           "single available injection compatibility preserved");
}

} // namespace

int main() {
    const auto model = compile_fixture();
    test_failure_mode_table();
    test_simultaneous_states(model);
    test_breaker_and_hold_up(model);
    test_degraded_and_unknown_modes(model);
    test_domain_endpoint_and_relation_injection(model);
    test_budget_and_invalid_input(model);
    return 0;
}
