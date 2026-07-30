#include "ltfa/availability.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>
#include <utility>
#include <vector>

namespace {

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

ltfa::AvailabilityState expected_all(ltfa::AvailabilityState left, ltfa::AvailabilityState right) {
    using State = ltfa::AvailabilityState;
    if (left == State::unavailable || right == State::unavailable) {
        return State::unavailable;
    }
    if (left == State::unknown || right == State::unknown) {
        return State::unknown;
    }
    if (left == State::degraded || right == State::degraded) {
        return State::degraded;
    }
    return State::available;
}

ltfa::AvailabilityState expected_any(ltfa::AvailabilityState left, ltfa::AvailabilityState right) {
    using State = ltfa::AvailabilityState;
    if (left == State::available || right == State::available) {
        return State::available;
    }
    if (left == State::degraded || right == State::degraded) {
        return State::degraded;
    }
    if (left == State::unknown || right == State::unknown) {
        return State::unknown;
    }
    return State::unavailable;
}

void test_truth_tables() {
    using State = ltfa::AvailabilityState;
    constexpr State states[]{
        State::available,
        State::degraded,
        State::unavailable,
        State::unknown,
    };

    expect(ltfa::evaluate_requirement_states(ltfa::RequirementMode::all, {}) == State::unknown,
           "an empty all group must fail closed as unknown");
    expect(ltfa::evaluate_requirement_states(ltfa::RequirementMode::any, {}) == State::unknown,
           "an empty any group must fail closed as unknown");
    expect(ltfa::evaluate_requirement_states(ltfa::RequirementMode::at_least, {}, 1) ==
               State::unknown,
           "an empty at-least group must fail closed as unknown");

    for (const auto left : states) {
        expect(ltfa::evaluate_requirement_states(ltfa::RequirementMode::all, {left}) == left,
               "a one-provider all group must preserve its state");
        expect(ltfa::evaluate_requirement_states(ltfa::RequirementMode::any, {left}) == left,
               "a one-provider any group must preserve its state");
        for (const auto right : states) {
            const std::vector providers{left, right};
            expect(ltfa::evaluate_requirement_states(ltfa::RequirementMode::all, providers) ==
                       expected_all(left, right),
                   "all truth table mismatch");
            expect(ltfa::evaluate_requirement_states(ltfa::RequirementMode::any, providers) ==
                       expected_any(left, right),
                   "any truth table mismatch");
        }
    }

    expect(ltfa::evaluate_requirement_states(ltfa::RequirementMode::at_least,
                                             {State::available, State::available, State::available},
                                             1) == State::available,
           "at-least must be available when the threshold is met without known member loss");
    expect(ltfa::evaluate_requirement_states(
               ltfa::RequirementMode::at_least,
               {State::available, State::unavailable, State::unavailable}, 1) == State::degraded,
           "at-least must expose known redundancy loss while its threshold remains met");
    expect(ltfa::evaluate_requirement_states(
               ltfa::RequirementMode::at_least,
               {State::available, State::degraded, State::unavailable}, 2) == State::degraded,
           "at-least must be degraded when degraded providers are needed for the threshold");
    expect(ltfa::evaluate_requirement_states(ltfa::RequirementMode::at_least,
                                             {State::available, State::unknown, State::unavailable},
                                             2) == State::unknown,
           "at-least must remain unknown when unknown evidence decides the threshold");
    expect(ltfa::evaluate_requirement_states(
               ltfa::RequirementMode::at_least,
               {State::available, State::unavailable, State::unavailable}, 2) == State::unavailable,
           "at-least must be unavailable when the threshold cannot be reached");
    expect(ltfa::evaluate_requirement_states(ltfa::RequirementMode::at_least, {State::available},
                                             0) == State::unknown,
           "at-least without a positive threshold must fail closed as unknown");
}

ltfa::StringId add_string(ltfa::NormalizedModel &model, std::string value) {
    const auto id = ltfa::StringId{static_cast<std::uint32_t>(model.strings.size())};
    model.strings.push_back(std::move(value));
    return id;
}

ltfa::NormalizedModel make_model() {
    ltfa::NormalizedModel model;
    const auto verified_scope = add_string(model, "scope:verified");
    const auto partial_scope = add_string(model, "scope:partial");
    const auto unknown_scope = add_string(model, "scope:unknown");
    const auto provider_a = add_string(model, "provider:a");
    const auto provider_b = add_string(model, "provider:b");
    const auto provider_c = add_string(model, "provider:c");
    const auto consumer = add_string(model, "service:consumer");
    const auto independent = add_string(model, "service:independent");
    const auto endpoint = add_string(model, "endpoint:z");
    const auto group_connectivity = add_string(model, "requirement:service:consumer:connectivity");
    const auto group_power = add_string(model, "requirement:service:consumer:power");
    const auto domain_cabinet = add_string(model, "cabinet:a");
    const auto domain_shared = add_string(model, "shared-service:a");
    const auto domain_unknown = add_string(model, "carrier:unknown");

    const auto add_scope = [&](ltfa::StringId external_id, ltfa::ScopeKind kind,
                               ltfa::Completeness completeness,
                               ltfa::EvidenceQuality evidence_quality) {
        ltfa::Scope value;
        value.id = ltfa::ScopeId{static_cast<std::uint32_t>(model.scopes.size())};
        value.external_id = external_id;
        value.kind = kind;
        value.completeness = completeness;
        value.evidence_quality = evidence_quality;
        model.scopes.push_back(value);
    };
    add_scope(verified_scope, ltfa::ScopeKind::site, ltfa::Completeness::verified,
              ltfa::EvidenceQuality::confirmed);
    add_scope(partial_scope, ltfa::ScopeKind::shared, ltfa::Completeness::partial,
              ltfa::EvidenceQuality::inferred);
    add_scope(unknown_scope, ltfa::ScopeKind::other, ltfa::Completeness::unknown,
              ltfa::EvidenceQuality::unknown);

    const auto add_entity = [&](ltfa::StringId external_id, ltfa::EntityKind kind,
                                ltfa::Plane plane, ltfa::Layer layer, ltfa::ScopeId scope) {
        ltfa::Entity value;
        value.id = ltfa::EntityId{static_cast<std::uint32_t>(model.entities.size())};
        value.external_id = external_id;
        value.kind = kind;
        value.plane = plane;
        value.layer = layer;
        value.scope = scope;
        model.entities.push_back(value);
    };
    add_entity(provider_a, ltfa::EntityKind::device, ltfa::Plane::facility, ltfa::Layer::physical,
               ltfa::ScopeId{0});
    add_entity(provider_b, ltfa::EntityKind::device, ltfa::Plane::communication,
               ltfa::Layer::physical, ltfa::ScopeId{0});
    add_entity(provider_c, ltfa::EntityKind::device, ltfa::Plane::communication,
               ltfa::Layer::physical, ltfa::ScopeId{1});
    add_entity(consumer, ltfa::EntityKind::service, ltfa::Plane::service, ltfa::Layer::application,
               ltfa::ScopeId{1});
    add_entity(independent, ltfa::EntityKind::service, ltfa::Plane::service,
               ltfa::Layer::application, ltfa::ScopeId{0});

    ltfa::Endpoint endpoint_value;
    endpoint_value.id = ltfa::EndpointId{0};
    endpoint_value.external_id = endpoint;
    endpoint_value.owner = ltfa::EntityId{2};
    endpoint_value.kind = ltfa::EndpointKind::network;
    endpoint_value.scope = ltfa::ScopeId{2};
    model.endpoints.push_back(endpoint_value);

    model.requirement_providers = {
        ltfa::EntityId{1},
        ltfa::EntityId{2},
        ltfa::EntityId{0},
        ltfa::EntityId{1},
    };
    const auto add_requirement = [&](ltfa::StringId external_id, ltfa::RequirementMode mode,
                                     ltfa::IndexRange providers, std::uint64_t hold_up_ms) {
        ltfa::RequirementGroup value;
        value.id =
            ltfa::RequirementGroupId{static_cast<std::uint32_t>(model.requirement_groups.size())};
        value.external_id = external_id;
        value.consumer = ltfa::EntityId{3};
        value.mode = mode;
        value.providers = providers;
        value.hold_up_ms = hold_up_ms;
        model.requirement_groups.push_back(value);
    };
    add_requirement(group_connectivity, ltfa::RequirementMode::any, {0, 2}, 0);
    add_requirement(group_power, ltfa::RequirementMode::all, {2, 2}, 30'000);

    model.failure_domain_members = {
        ltfa::ObjectRef::from(ltfa::EntityId{1}),   ltfa::ObjectRef::from(ltfa::EntityId{0}),
        ltfa::ObjectRef::from(ltfa::EntityId{3}),   ltfa::ObjectRef::from(ltfa::EntityId{1}),
        ltfa::ObjectRef::from(ltfa::EndpointId{0}),
    };
    const auto add_domain = [&](ltfa::StringId external_id, ltfa::FailureDomainKind kind,
                                ltfa::IndexRange members) {
        ltfa::FailureDomain value;
        value.id = ltfa::FailureDomainId{static_cast<std::uint32_t>(model.failure_domains.size())};
        value.external_id = external_id;
        value.kind = kind;
        value.members = members;
        model.failure_domains.push_back(value);
    };
    add_domain(domain_cabinet, ltfa::FailureDomainKind::cabinet, {0, 2});
    add_domain(domain_shared, ltfa::FailureDomainKind::shared_service, {2, 2});
    add_domain(domain_unknown, ltfa::FailureDomainKind::carrier, {4, 1});
    return model;
}

void test_requirement_evaluation() {
    using State = ltfa::AvailabilityState;
    const auto model = make_model();
    auto snapshot = ltfa::make_availability_snapshot(model);
    expect(snapshot.set(ltfa::ObjectRef::from(ltfa::EntityId{1}), State::degraded),
           "provider state must be writable");
    expect(snapshot.set(ltfa::ObjectRef::from(ltfa::EntityId{2}), State::unknown),
           "provider state must be writable");

    const auto connectivity =
        ltfa::evaluate_requirement_group(model, ltfa::RequirementGroupId{0}, snapshot);
    const auto power =
        ltfa::evaluate_requirement_group(model, ltfa::RequirementGroupId{1}, snapshot);
    expect(connectivity.has_value() && connectivity->state == State::degraded &&
               connectivity->hold_up_ms == 0,
           "any group must prefer degraded over unknown");
    expect(power.has_value() && power->state == State::degraded && power->hold_up_ms == 30'000,
           "all group must retain degraded state and fixed hold-up metadata");

    const auto combined = ltfa::evaluate_consumer_requirements(model, ltfa::EntityId{3}, snapshot);
    expect(combined.has_value() && combined->state == State::degraded,
           "multiple required groups must combine with all semantics");
    expect(combined.has_value() && combined->groups.size() == 2 &&
               combined->groups[0].group == ltfa::RequirementGroupId{0} &&
               combined->groups[1].group == ltfa::RequirementGroupId{1},
           "group evaluations must retain normalized deterministic order");
    expect(combined.has_value() &&
               combined->qualification.completeness == ltfa::Completeness::partial &&
               combined->qualification.evidence_quality == ltfa::EvidenceQuality::inferred,
           "consumer evaluation must retain its scope qualification");

    expect(snapshot.set(ltfa::ObjectRef::from(ltfa::EntityId{0}), State::unavailable),
           "provider state must be writable");
    const auto failed = ltfa::evaluate_consumer_requirements(model, ltfa::EntityId{3}, snapshot);
    expect(failed.has_value() && failed->state == State::unavailable,
           "one unavailable all-group provider must make the combined result unavailable");

    ltfa::AvailabilitySnapshot partial_snapshot;
    partial_snapshot.entity_states.push_back(State::available);
    const auto unknown =
        ltfa::evaluate_requirement_group(model, ltfa::RequirementGroupId{1}, partial_snapshot);
    expect(unknown.has_value() && unknown->state == State::unknown,
           "missing provider state must remain conservatively unknown");

    const auto no_requirements =
        ltfa::evaluate_consumer_requirements(model, ltfa::EntityId{4}, snapshot);
    expect(no_requirements.has_value() && no_requirements->state == State::available &&
               no_requirements->groups.empty(),
           "an entity without requirement groups has no unsatisfied requirement");

    expect(!ltfa::evaluate_requirement_group(model, ltfa::RequirementGroupId{99}, snapshot)
                .has_value(),
           "invalid requirement IDs must fail without an invented state");
    expect(!ltfa::evaluate_consumer_requirements(model, ltfa::EntityId{99}, snapshot).has_value(),
           "invalid consumer IDs must fail without an invented result");

    auto threshold_model = make_model();
    ltfa::RequirementGroup threshold;
    threshold.id = ltfa::RequirementGroupId{2};
    threshold.external_id = threshold_model.requirement_groups.front().external_id;
    threshold.consumer = ltfa::EntityId{4};
    threshold.mode = ltfa::RequirementMode::at_least;
    threshold.minimum_providers = 2;
    threshold.providers = {0, 2};
    threshold_model.requirement_groups.push_back(threshold);
    auto threshold_snapshot = ltfa::make_availability_snapshot(threshold_model);
    expect(threshold_snapshot.set(ltfa::ObjectRef::from(ltfa::EntityId{2}), State::degraded),
           "threshold provider state must be writable");
    const auto threshold_degraded = ltfa::evaluate_requirement_group(
        threshold_model, ltfa::RequirementGroupId{2}, threshold_snapshot);
    expect(threshold_degraded.has_value() && threshold_degraded->state == State::degraded,
           "normalized at-least group must use its minimum provider count");
    expect(threshold_snapshot.set(ltfa::ObjectRef::from(ltfa::EntityId{2}), State::unavailable),
           "threshold provider state must be writable");
    const auto threshold_failed = ltfa::evaluate_requirement_group(
        threshold_model, ltfa::RequirementGroupId{2}, threshold_snapshot);
    expect(threshold_failed.has_value() && threshold_failed->state == State::unavailable,
           "normalized at-least group must fail when its minimum cannot be met");
}

void test_failure_domains() {
    using State = ltfa::AvailabilityState;
    const auto model = make_model();

    const auto cabinet = ltfa::expand_failure_domain(model, ltfa::FailureDomainId{0});
    expect(cabinet.has_value() && cabinet->members.size() == 2,
           "failure domain must expand every member");
    expect(cabinet.has_value() &&
               model.external_id(cabinet->members[0]) == std::string_view{"provider:a"} &&
               model.external_id(cabinet->members[1]) == std::string_view{"provider:b"},
           "failure-domain expansion must be ordered by stable external ID");
    expect(cabinet.has_value() &&
               cabinet->qualification.completeness == ltfa::Completeness::verified &&
               cabinet->qualification.evidence_quality == ltfa::EvidenceQuality::confirmed,
           "verified domain members must retain confirmed qualification");

    const auto shared = ltfa::expand_failure_domain(model, ltfa::FailureDomainId{1});
    expect(shared.has_value() && shared->members.size() == 2,
           "an object may participate in a second failure domain");
    expect(shared.has_value() &&
               shared->qualification.completeness == ltfa::Completeness::partial &&
               shared->qualification.evidence_quality == ltfa::EvidenceQuality::inferred,
           "domain qualification must conservatively aggregate member scopes");

    const auto unknown = ltfa::expand_failure_domain(model, ltfa::FailureDomainId{2});
    expect(unknown.has_value() &&
               unknown->qualification.completeness == ltfa::Completeness::unknown &&
               unknown->qualification.evidence_quality == ltfa::EvidenceQuality::unknown,
           "unknown member qualification must remain unknown");

    auto snapshot = ltfa::make_availability_snapshot(model);
    expect(ltfa::apply_failure_domain_failure(model, ltfa::FailureDomainId{1}, snapshot),
           "valid failure-domain failure must apply");
    expect(snapshot.state(ltfa::ObjectRef::from(ltfa::EntityId{1})) == State::unavailable &&
               snapshot.state(ltfa::ObjectRef::from(ltfa::EntityId{3})) == State::unavailable &&
               snapshot.state(ltfa::ObjectRef::from(ltfa::EntityId{0})) == State::available,
           "domain failure must affect exactly its current members");
    expect(ltfa::apply_failure_domain_failure(model, ltfa::FailureDomainId{2}, snapshot) &&
               snapshot.state(ltfa::ObjectRef::from(ltfa::EndpointId{0})) == State::unavailable,
           "domain failure must apply to endpoint members");

    auto unchanged = snapshot;
    expect(!ltfa::apply_failure_domain_failure(model, ltfa::FailureDomainId{99}, unchanged),
           "invalid domain IDs must fail");
    expect(unchanged.entity_states == snapshot.entity_states &&
               unchanged.endpoint_states == snapshot.endpoint_states,
           "invalid domain application must not partially mutate state");

    ltfa::AvailabilitySnapshot undersized;
    undersized.entity_states.assign(model.entities.size() - 1, State::available);
    undersized.endpoint_states.assign(model.endpoints.size(), State::available);
    const auto undersized_before = undersized;
    expect(!ltfa::apply_failure_domain_failure(model, ltfa::FailureDomainId{1}, undersized) &&
               undersized.entity_states == undersized_before.entity_states &&
               undersized.endpoint_states == undersized_before.endpoint_states,
           "undersized snapshots must fail before domain state is mutated");
}

} // namespace

int main() {
    test_truth_tables();
    test_requirement_evaluation();
    test_failure_domains();
    return 0;
}
