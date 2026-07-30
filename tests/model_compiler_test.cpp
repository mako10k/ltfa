#include "ltfa/dsl/parser.hpp"
#include "ltfa/model_compiler.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

int failures = 0;
constexpr ltfa::StringId kFile{3};

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << message << '\n';
        ++failures;
    }
}

ltfa::CompileResult compile(std::string_view source,
                            const ltfa::ModelLimits &limits = ltfa::kDefaultModelLimits) {
    auto parsed = ltfa::dsl::parse_document(source, kFile, limits);
    if (!parsed.ok()) {
        if (parsed.error.has_value()) {
            std::cerr << "fixture parse failed: " << ltfa::to_string(parsed.error->code) << ": "
                      << parsed.error->message << '\n';
        }
        ++failures;
        return {};
    }
    ltfa::dsl::LoadedDocument loaded;
    loaded.statements = std::move(parsed.document->statements);
    loaded.total_source_bytes = source.size();
    return ltfa::compile_model(loaded, limits);
}

std::optional<ltfa::Diagnostic>
expect_error(std::string_view source, ltfa::DiagnosticCode code,
             const ltfa::ModelLimits &limits = ltfa::kDefaultModelLimits) {
    const auto result = compile(source, limits);
    expect(!result.model.has_value(), "failed compilation must not expose a partial model");
    if (!result.error.has_value()) {
        std::cerr << "expected diagnostic " << ltfa::to_string(code) << '\n';
        ++failures;
        return std::nullopt;
    }
    if (result.error->code != code) {
        std::cerr << "expected diagnostic " << ltfa::to_string(code) << ", got "
                  << ltfa::to_string(result.error->code) << ": " << result.error->message << '\n';
        ++failures;
    }
    return result.error;
}

template <typename Value>
std::size_t find_by_external_id(const ltfa::NormalizedModel &model,
                                const std::vector<Value> &values, std::string_view external_id) {
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (model.string(values[index].external_id) == external_id) {
            return index;
        }
    }
    return values.size();
}

std::string valid_model_source() {
    return "ltfa 1\n"
           "entity service:monitor kind=service plane=service layer=application scope=site:a "
           "x-owner=ops\n"
           "scope site:a kind=site completeness=partial evidence-quality=confirmed\n"
           "entity device:switch kind=device plane=communication layer=physical scope=site:a\n"
           "entity facility:panel kind=facility plane=facility layer=physical scope=site:a "
           "failure-domain=panel-a\n"
           "entity device:router kind=device plane=communication layer=physical scope=site:a "
           "failure-domain=cabinet-a protocol=modbus-rtu\n"
           "entity network:lan kind=network-space plane=communication layer=l3 scope=site:a\n"
           "endpoint endpoint:panel:out owner=facility:panel kind=power-output\n"
           "endpoint endpoint:router:power owner=device:router kind=power-input "
           "failure-domain=cabinet-a\n"
           "endpoint endpoint:router:wan owner=device:router kind=network "
           "network-space=network:lan address=\"192.0.2.10\"\n"
           "endpoint endpoint:switch:port owner=device:switch kind=network "
           "network-space=network:lan\n"
           "connect endpoint:panel:out -> endpoint:router:power kind=power\n"
           "connect endpoint:router:wan -- endpoint:switch:port kind=network label=lan\n"
           "relate service:monitor -> device:switch kind=depends_on\n"
           "relate network:lan -> device:switch kind=uses_underlay\n"
           "relate device:router -> facility:panel kind=powered_by\n"
           "require service:monitor by device:router group=runtime mode=all hold-up=30s\n"
           "require service:monitor by device:switch group=runtime mode=all hold-up=30s\n"
           "require service:monitor by device:router group=backup mode=any\n"
           "failure failure:router target=device:router mode=device-failure after=2s\n"
           "failure failure:custom target=device:switch mode=x-maintenance\n"
           "probe probe:wan target=endpoint:router:wan vantage=device:switch kind=icmp "
           "valid-for=60s\n";
}

void test_normalized_model() {
    const auto result = compile(valid_model_source());
    expect(result.ok(), "complete model must compile");
    if (!result.ok()) {
        if (result.error.has_value()) {
            std::cerr << ltfa::to_string(result.error->code) << ": " << result.error->message
                      << '\n';
        }
        return;
    }
    const auto &model = *result.model;
    expect(model.schema_major_version == 1 && model.failure_mode_catalog_version == 1,
           "normalized schema and failure-mode catalog versions must be explicit");
    expect(model.scopes.size() == 1, "scope must be normalized");
    expect(model.entities.size() == 5, "entities must be normalized");
    expect(model.endpoints.size() == 4, "endpoints must be normalized");
    expect(model.relations.size() == 5, "connections and logical relations must be normalized");
    expect(model.requirement_groups.size() == 2, "named requirement groups must remain separate");
    expect(model.requirement_providers.size() == 3, "merged providers must be retained");
    expect(model.failure_domains.size() == 2, "failure domains must be synthesized");
    expect(model.failure_modes.size() == 2, "built-in and extension failures must be normalized");
    expect(model.probes.size() == 1, "probe must be normalized");

    expect(model.string(model.entities.front().external_id) == "device:router",
           "entities must be sorted by stable external ID");
    const auto service_index =
        find_by_external_id(model, model.entities, std::string_view{"service:monitor"});
    const auto router_index =
        find_by_external_id(model, model.entities, std::string_view{"device:router"});
    const auto wan_index =
        find_by_external_id(model, model.endpoints, std::string_view{"endpoint:router:wan"});
    expect(service_index < model.entities.size(), "service must be addressable by external ID");
    expect(router_index < model.entities.size(), "router must be addressable by external ID");
    expect(wan_index < model.endpoints.size(), "WAN endpoint must be addressable by external ID");
    if (service_index < model.entities.size()) {
        expect(model.entities[service_index].attributes.count == 1,
               "extension attributes must survive normalization");
        const auto &attribute = model.attributes[model.entities[service_index].attributes.begin];
        expect(model.string(attribute.key) == "x-owner" && attribute.extension,
               "extension key and marker must survive normalization");
    }
    if (wan_index < model.endpoints.size()) {
        expect(model.endpoints[wan_index].network_space.is_valid(),
               "endpoint network-space reference must be retained");
        expect(model.endpoints[wan_index].attributes.count == 1,
               "address metadata must survive normalization");
    }

    const auto runtime = std::find_if(
        model.requirement_groups.begin(), model.requirement_groups.end(), [&](const auto &group) {
            return model.string(group.external_id).find("runtime") != std::string_view::npos;
        });
    const auto backup = std::find_if(
        model.requirement_groups.begin(), model.requirement_groups.end(), [&](const auto &group) {
            return model.string(group.external_id).find("backup") != std::string_view::npos;
        });
    expect(runtime != model.requirement_groups.end() && runtime->mode == ltfa::RequirementMode::all,
           "all-provider requirement mode must be normalized");
    expect(runtime != model.requirement_groups.end() && runtime->hold_up_ms == 30'000,
           "requirement duration must use milliseconds");
    expect(backup != model.requirement_groups.end() && backup->mode == ltfa::RequirementMode::any,
           "any-provider requirement mode must be normalized");
    expect(model.string(model.requirement_groups.front().external_id).rfind("requirement:", 0) == 0,
           "requirement group must receive a deterministic derived ID");
    expect(model.string(model.relations.front().external_id).rfind("relation:", 0) == 0,
           "relations must receive deterministic derived IDs");
    const auto router_failure =
        find_by_external_id(model, model.failure_modes, std::string_view{"failure:router"});
    const auto custom_failure =
        find_by_external_id(model, model.failure_modes, std::string_view{"failure:custom"});
    expect(router_failure < model.failure_modes.size() &&
               model.failure_modes[router_failure].delay_ms == 2'000,
           "failure delay must use milliseconds");
    expect(custom_failure < model.failure_modes.size() &&
               model.failure_modes[custom_failure].kind == ltfa::FailureModeKind::extension &&
               model.string(model.failure_modes[custom_failure].extension_kind) == "x-maintenance",
           "unknown x- failure modes must be preserved without built-in semantics");
    expect(model.probes.front().valid_for_ms == 60'000, "probe freshness must use milliseconds");

    expect(model.failure_domains.front().kind == ltfa::FailureDomainKind::cabinet,
           "failure-domain kind must be inferred deterministically");
    expect(model.failure_domains.front().members.count == 2,
           "failure-domain many-to-many members must be indexed");
    expect(model.availability_dependencies.size() == 7,
           "power, logical, and requirement availability edges must be combined");
    expect(model.entity_availability_adjacency.size() == model.entities.size(),
           "entity availability adjacency must cover every entity");
    expect(model.endpoint_relation_adjacency.size() == model.endpoints.size(),
           "endpoint relation adjacency must cover every endpoint");

    if (router_index < model.entities.size()) {
        const auto &adjacency = model.entity_availability_adjacency[router_index];
        expect(adjacency.forward.count == 2,
               "requirement provider must retain one edge per requirement group");
    }
}

void test_diagnostics() {
    const std::string prefix = "ltfa 1\nscope s\n";
    const auto duplicate = expect_error(prefix + "entity same scope=s\nendpoint same owner=same\n",
                                        ltfa::DiagnosticCode::id_duplicate);
    expect(duplicate.has_value() && duplicate->secondary_source.has_value(),
           "duplicate IDs must report the original declaration");

    expect_error(prefix + "entity e scope=s\nendpoint p owner=missing kind=network\n",
                 ltfa::DiagnosticCode::reference_unresolved);
    expect_error(prefix + "entity e kind=device plane=communication layer=l3 scope=s "
                          "network-space=missing address=192.0.2.1\n",
                 ltfa::DiagnosticCode::reference_unresolved);
    expect_error(prefix + "entity e kind=device plane=facility layer=application scope=s\n",
                 ltfa::DiagnosticCode::layer_incompatible);
    expect_error(prefix + "entity source kind=facility plane=facility layer=physical scope=s\n"
                          "entity sink kind=device plane=communication layer=physical scope=s\n"
                          "endpoint out owner=source kind=power-output\n"
                          "endpoint in owner=sink kind=power-input\n"
                          "connect out -- in kind=power\n",
                 ltfa::DiagnosticCode::endpoint_incompatible);
    expect_error(prefix + "entity upper kind=device plane=communication layer=l2 scope=s\n"
                          "entity lower kind=device plane=communication layer=l3 scope=s\n"
                          "relate upper -> lower kind=uses_underlay\n",
                 ltfa::DiagnosticCode::layer_incompatible);
    expect_error(prefix + "entity upper kind=device plane=communication layer=l2 scope=s\n"
                          "entity lower kind=device plane=communication layer=l3 scope=s\n"
                          "endpoint upper-p owner=upper kind=network\n"
                          "endpoint lower-p owner=lower kind=network\n"
                          "connect upper-p -- lower-p kind=network\n",
                 ltfa::DiagnosticCode::layer_incompatible);
    expect_error(prefix + "entity facility kind=facility plane=facility layer=physical scope=s\n"
                          "endpoint invalid owner=facility kind=network\n",
                 ltfa::DiagnosticCode::endpoint_incompatible);
    expect_error(prefix + "entity a kind=device plane=communication layer=l3 scope=s\n"
                          "entity b kind=device plane=communication layer=l3 scope=s\n"
                          "relate a -> b kind=depends_on\n"
                          "relate b -> a kind=depends_on\n",
                 ltfa::DiagnosticCode::dependency_cycle);
    expect_error(prefix + "entity a scope=s\n"
                          "entity b scope=s\n"
                          "require a by b group=g mode=all hold-up=99999999999999999999d\n",
                 ltfa::DiagnosticCode::dsl_invalid_value);

    ltfa::ModelLimits limits = ltfa::kDefaultModelLimits;
    limits.max_entities = 1;
    expect_error(prefix + "entity a scope=s\nentity b scope=s\n",
                 ltfa::DiagnosticCode::limit_exceeded, limits);
    limits = ltfa::kDefaultModelLimits;
    limits.max_total_source_bytes = 3;
    expect_error(prefix, ltfa::DiagnosticCode::limit_total_source_bytes, limits);
    expect_error("ltfa 1\ninclude \"still-unresolved.ltfa\"\n",
                 ltfa::DiagnosticCode::include_invalid);

    const auto communication_application =
        compile(prefix + "entity app kind=process plane=communication layer=application scope=s\n");
    expect(communication_application.ok(),
           "communication plane must support the application layer");
}

void test_multidrop_bus() {
    const auto result =
        compile("ltfa 1\n"
                "scope s\n"
                "entity bus kind=bus plane=control-signal layer=physical scope=s\n"
                "entity logger kind=device plane=control-signal layer=physical scope=s\n"
                "entity meter kind=device plane=control-signal layer=physical scope=s\n"
                "endpoint logger-p owner=logger kind=serial\n"
                "endpoint meter-p owner=meter kind=serial\n"
                "connect logger-p -- bus kind=serial\n"
                "connect meter-p -- bus kind=serial\n");
    expect(result.ok(), "multidrop serial must compile through a first-class bus entity");
    if (result.ok()) {
        expect(result.model->relations.size() == 2,
               "multidrop bus must retain each endpoint-to-bus connection");
    }
}

void test_physical_cycles_and_determinism() {
    const std::string declarations =
        "scope s\n"
        "entity a kind=device plane=communication layer=physical scope=s\n"
        "entity b kind=device plane=communication layer=physical scope=s\n"
        "entity c kind=device plane=communication layer=physical scope=s\n"
        "endpoint a-p owner=a kind=network\n"
        "endpoint b-p owner=b kind=network\n"
        "endpoint c-p owner=c kind=network\n";
    const std::string relations = "connect a-p -- b-p kind=network\n"
                                  "connect b-p -- c-p kind=network\n"
                                  "connect c-p -- a-p kind=network\n";
    const auto first = compile("ltfa 1\n" + declarations + relations);
    const auto second = compile("ltfa 1\n"
                                "entity c kind=device plane=communication layer=physical scope=s\n"
                                "endpoint c-p owner=c kind=network\n"
                                "scope s\n"
                                "entity b kind=device plane=communication layer=physical scope=s\n"
                                "endpoint b-p owner=b kind=network\n"
                                "entity a kind=device plane=communication layer=physical scope=s\n"
                                "endpoint a-p owner=a kind=network\n" +
                                relations);
    expect(first.ok() && second.ok(), "physical topology cycles must remain valid");
    if (!first.ok() || !second.ok()) {
        return;
    }
    expect(first.model->strings == second.model->strings,
           "interned string order must be declaration-order independent");
    expect(first.model->relations.size() == second.model->relations.size(),
           "deterministic compilations must retain the same relation count");
    for (std::size_t index = 0; index < first.model->relations.size(); ++index) {
        expect(first.model->string(first.model->relations[index].external_id) ==
                   second.model->string(second.model->relations[index].external_id),
               "derived relation order must be declaration-order independent");
    }

    expect_error("ltfa 1\n" + declarations +
                     "connect a-p -- b-p kind=network\n"
                     "connect b-p -- a-p kind=network\n",
                 ltfa::DiagnosticCode::id_duplicate);
}

void test_link_aggregation_and_control_domains() {
    const auto result =
        compile("ltfa 1\n"
                "scope s completeness=verified evidence-quality=confirmed\n"
                "entity sw1 kind=device plane=communication layer=physical scope=s\n"
                "entity sw2 kind=device plane=communication layer=physical scope=s\n"
                "endpoint sw1:p1 owner=sw1 kind=network\n"
                "endpoint sw1:p2 owner=sw1 kind=network\n"
                "endpoint sw2:p1 owner=sw2 kind=network\n"
                "endpoint sw2:p2 owner=sw2 kind=network\n"
                "entity link:a kind=link plane=communication layer=physical scope=s "
                "failure-domain=cable-route:east\n"
                "entity link:b kind=link plane=communication layer=physical scope=s "
                "failure-domain=cable-route:west\n"
                "connect sw1:p1 -- link:a kind=network\n"
                "connect link:a -- sw2:p1 kind=network\n"
                "connect sw1:p2 -- link:b kind=network\n"
                "connect link:b -- sw2:p2 kind=network\n"
                "entity lag:uplink kind=aggregate plane=communication layer=l2 scope=s "
                "protocol=lacp mode=active-active\n"
                "relate link:a -> lag:uplink kind=member_of role=active "
                "normal-state=forwarding\n"
                "relate link:b -> lag:uplink kind=member_of role=active "
                "normal-state=forwarding\n"
                "require lag:uplink by link:a group=members mode=at-least minimum=1\n"
                "require lag:uplink by link:b group=members mode=at-least minimum=1\n"
                "entity stp:default kind=control-domain plane=communication layer=l2 scope=s "
                "protocol=rstp\n"
                "relate link:a -> stp:default kind=member_of role=designated "
                "normal-state=forwarding\n"
                "relate link:b -> stp:default kind=member_of role=alternate "
                "normal-state=discarding\n"
                "failure failure:link-a target=link:a mode=link-down\n");
    expect(result.ok(), "first-class links, aggregate policy, and a control domain must compile");
    if (!result.ok()) {
        if (result.error.has_value()) {
            std::cerr << ltfa::to_string(result.error->code) << ": " << result.error->message
                      << '\n';
        }
        return;
    }

    const auto &model = *result.model;
    const auto link_a = find_by_external_id(model, model.entities, "link:a");
    const auto aggregate = find_by_external_id(model, model.entities, "lag:uplink");
    const auto control = find_by_external_id(model, model.entities, "stp:default");
    expect(link_a < model.entities.size() && model.entities[link_a].kind == ltfa::EntityKind::link,
           "physical link must remain a typed first-class entity");
    expect(aggregate < model.entities.size() &&
               model.entities[aggregate].kind == ltfa::EntityKind::aggregate,
           "aggregate must remain a typed first-class entity");
    expect(control < model.entities.size() &&
               model.entities[control].kind == ltfa::EntityKind::control_domain,
           "control domain must remain a typed first-class entity");
    expect(model.requirement_groups.size() == 1 &&
               model.requirement_groups.front().mode == ltfa::RequirementMode::at_least &&
               model.requirement_groups.front().minimum_providers == 1 &&
               model.requirement_groups.front().providers.count == 2,
           "aggregate min-active policy must normalize as one at-least group");
    expect(model.failure_modes.size() == 1 &&
               model.failure_modes.front().target ==
                   ltfa::ObjectRef::from(ltfa::EntityId{static_cast<std::uint32_t>(link_a)}),
           "a declared link failure must target the link entity");

    const auto alternate =
        std::find_if(model.relations.begin(), model.relations.end(), [&](const auto &relation) {
            return model.external_id(relation.from) == "link:b" &&
                   model.external_id(relation.to) == "stp:default";
        });
    expect(alternate != model.relations.end() && alternate->attributes.count == 2,
           "domain-specific role and normal state must survive on membership");
}

void test_link_aggregation_diagnostics() {
    const std::string prefix = "ltfa 1\n"
                               "scope s\n"
                               "entity sw kind=device plane=communication layer=physical scope=s\n"
                               "endpoint sw:p owner=sw kind=network\n";
    expect_error(prefix + "entity link:a kind=link plane=communication layer=physical scope=s\n"
                          "connect sw:p -- link:a kind=network\n",
                 ltfa::DiagnosticCode::endpoint_incompatible);
    expect_error("ltfa 1\n"
                 "scope s\n"
                 "entity link:a kind=link plane=communication layer=l2 scope=s\n",
                 ltfa::DiagnosticCode::layer_incompatible);
    expect_error("ltfa 1\n"
                 "scope s\n"
                 "entity lag:a kind=aggregate plane=communication layer=l2 scope=s "
                 "protocol=lacp\n",
                 ltfa::DiagnosticCode::dsl_missing_required_field);
    expect_error("ltfa 1\n"
                 "scope s\n"
                 "entity consumer scope=s\n"
                 "entity provider scope=s\n"
                 "require consumer by provider group=g mode=at-least\n",
                 ltfa::DiagnosticCode::dsl_missing_required_field);
    expect_error("ltfa 1\n"
                 "scope s\n"
                 "entity consumer scope=s\n"
                 "entity provider scope=s\n"
                 "require consumer by provider group=g mode=at-least minimum=2\n",
                 ltfa::DiagnosticCode::dsl_invalid_value);
    expect_error("ltfa 1\n"
                 "scope s\n"
                 "entity consumer scope=s\n"
                 "entity provider scope=s\n"
                 "require consumer by provider group=g mode=any minimum=1\n",
                 ltfa::DiagnosticCode::dsl_invalid_attribute);
}

} // namespace

int main() {
    test_normalized_model();
    test_diagnostics();
    test_multidrop_bus();
    test_physical_cycles_and_determinism();
    test_link_aggregation_and_control_domains();
    test_link_aggregation_diagnostics();
    return failures == 0 ? 0 : 1;
}
