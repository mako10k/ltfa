#include "ltfa/dsl/parser.hpp"
#include "ltfa/model_compiler.hpp"
#include "ltfa/model_output.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace {

int failures = 0;
constexpr ltfa::StringId kFile{4};

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << message << '\n';
        ++failures;
    }
}

std::optional<ltfa::NormalizedModel> compile(std::string_view source) {
    auto parsed = ltfa::dsl::parse_document(source, kFile);
    if (!parsed.ok()) {
        if (parsed.error.has_value()) {
            std::cerr << "fixture parse failed: " << ltfa::to_string(parsed.error->code) << ": "
                      << parsed.error->message << '\n';
        }
        ++failures;
        return std::nullopt;
    }
    ltfa::dsl::LoadedDocument loaded;
    loaded.statements = std::move(parsed.document->statements);
    loaded.total_source_bytes = source.size();
    auto compiled = ltfa::compile_model(loaded);
    if (!compiled.ok()) {
        if (compiled.error.has_value()) {
            std::cerr << "fixture compile failed: " << ltfa::to_string(compiled.error->code) << ": "
                      << compiled.error->message << '\n';
        }
        ++failures;
        return std::nullopt;
    }
    return std::move(compiled.model);
}

std::string minimal_source(bool reordered = false) {
    if (reordered) {
        return "ltfa 1\n"
               "entity e kind=device plane=communication layer=physical scope=s "
               "label=\"Edge A\" x-rank=1\n"
               "scope s kind=site label=\"Site A\" completeness=verified "
               "evidence-quality=confirmed x-note=reviewed\n";
    }
    return "ltfa 1\n"
           "scope s kind=site label=\"Site A\" completeness=verified "
           "evidence-quality=confirmed x-note=reviewed\n"
           "entity e kind=device plane=communication layer=physical scope=s "
           "label=\"Edge A\" x-rank=1\n";
}

void test_golden_outputs() {
    const auto model = compile(minimal_source());
    if (!model.has_value()) {
        return;
    }
    ltfa::ModelOutputContext context;
    context.model_inputs = {"z.ltfa", "a.ltfa", "a.ltfa"};

    const std::string expected_human =
        "result: complete\n"
        "schema: ltfa.model/v1\n"
        "operation: model-summary\n"
        "focus: registered topology model\n"
        "confidence: confirmed=1 inferred=0 unknown=0\n"
        "analysis-at: not-set\n"
        "model-inputs:\n"
        "  - a.ltfa\n"
        "  - z.ltfa\n"
        "observation-inputs:\n"
        "  - none\n"
        "summary:\n"
        "  scopes: 1\n"
        "  entities: 1\n"
        "  endpoints: 0\n"
        "  relations: 0\n"
        "  requirement-groups: 0\n"
        "  failure-domains: 0\n"
        "  failure-modes: 0\n"
        "  probes: 0\n"
        "scopes:\n"
        "  - s kind=site completeness=verified evidence=confirmed label=\"Site A\"\n"
        "entities:\n"
        "  - e kind=device plane=communication layer=physical scope=s label=\"Edge A\"\n"
        "endpoints:\n"
        "relations:\n"
        "unknowns:\n"
        "  - none\n"
        "omissions:\n"
        "  restricted-objects: 0\n"
        "  evidence-references: 0\n";
    expect(ltfa::render_model_human(*model, context) == expected_human,
           "human model output must remain byte-stable");

    const std::string expected_json =
        "{\"schema\":\"ltfa.model/v1\",\"operation\":\"model-summary\",\"analysis_at\":null,"
        "\"model_inputs\":[\"a.ltfa\",\"z.ltfa\"],\"observation_inputs\":[],\"status\":"
        "\"complete\",\"findings\":[],\"warnings\":[],\"resource_budget\":null,\"omissions\":"
        "{\"restricted_objects\":0,\"evidence_references\":0},\"model\":{\"schema_major_"
        "version\":1,\"failure_mode_catalog_version\":1,\"counts\":{\"scopes\":1,\"entities\":"
        "1,\"endpoints\":0,\"relations\":0,\"requirement_groups\":0,\"failure_domains\":0,"
        "\"failure_modes\":0,\"probes\":0},\"scopes\":[{\"id\":\"s\",\"label\":\"Site A\","
        "\"kind\":\"site\",\"completeness\":\"verified\",\"evidence_quality\":\"confirmed\","
        "\"attributes\":[{\"key\":\"x-note\",\"kind\":\"identifier\",\"value\":\"reviewed\","
        "\"extension\":true}]}],\"entities\":[{\"id\":\"e\",\"label\":\"Edge A\",\"kind\":"
        "\"device\",\"plane\":\"communication\",\"layer\":\"physical\",\"scope\":\"s\","
        "\"network_space\":null,\"failure_domains\":[],\"sensitivity\":\"internal\","
        "\"attributes\":[{\"key\":\"x-rank\",\"kind\":\"integer\",\"value\":\"1\","
        "\"extension\":true}]}],\"endpoints\":[],\"relations\":[],\"requirement_groups\":[],"
        "\"failure_domains\":[],\"failure_modes\":[],\"probes\":[]}}\n";
    expect(ltfa::render_model_json(*model, context) == expected_json,
           "JSON model output must remain byte-stable");

    const std::string expected_dot =
        "digraph ltfa_model {\n"
        "  graph [ltfa_schema=\"ltfa.model.dot/v1\",operation=\"model\",model_inputs=\"[\\\"a.ltfa"
        "\\\",\\\"z.ltfa\\\"]\",observation_inputs=\"[]\",analysis_at=\"null\",restricted_"
        "omissions=\"0\"];\n"
        "  node [fontname=\"sans-serif\"];\n"
        "  edge [fontname=\"sans-serif\"];\n"
        "  subgraph \"cluster:scope:s\" {\n"
        "    label=\"scope s\";\n"
        "    \"e\" [label=\"Edge A\\ne\",shape=box,color=\"#2563eb\",style=\"rounded\"];\n"
        "  }\n"
        "}\n";
    expect(ltfa::render_model_dot(*model, context) == expected_dot,
           "DOT model output must remain byte-stable");
}

std::string restricted_source() {
    return "ltfa 1\n"
           "scope s kind=site completeness=partial evidence-quality=inferred\n"
           "entity public kind=device plane=communication layer=physical scope=s "
           "source-ref=evidence:public\n"
           "entity peer kind=device plane=communication layer=physical scope=s\n"
           "entity hidden kind=device plane=communication layer=physical scope=s "
           "sensitivity=restricted source-ref=evidence:hidden failure-domain=cabinet-secret\n"
           "endpoint public:p owner=public kind=network source-ref=evidence:endpoint\n"
           "endpoint peer:p owner=peer kind=network\n"
           "endpoint hidden:p owner=hidden kind=network sensitivity=restricted\n"
           "connect public:p -- peer:p kind=network source-ref=evidence:relation\n"
           "connect public:p -- hidden:p kind=network source-ref=evidence:hidden-relation\n"
           "require public by hidden group=restricted-provider mode=all\n"
           "failure failure:hidden target=hidden mode=device-failure\n"
           "probe probe:hidden target=hidden vantage=public kind=icmp valid-for=30s\n";
}

void test_restricted_policy() {
    const auto model = compile(restricted_source());
    if (!model.has_value()) {
        return;
    }
    const auto default_json = ltfa::render_model_json(*model);
    const auto default_human = ltfa::render_model_human(*model);
    const auto default_dot = ltfa::render_model_dot(*model);
    expect(default_json.find("\"hidden\"") == std::string::npos,
           "default JSON must omit restricted object IDs");
    expect(default_human.find("hidden") == std::string::npos,
           "default human output must omit restricted object IDs");
    expect(default_dot.find("\"hidden\"") == std::string::npos,
           "default DOT must omit restricted object IDs");
    expect(default_json.find("evidence:public") == std::string::npos &&
               default_json.find("evidence:endpoint") == std::string::npos &&
               default_json.find("evidence:relation") == std::string::npos,
           "default JSON must omit evidence references");
    expect(default_json.find("\"restricted_objects\":7") != std::string::npos,
           "default JSON must count restricted dependent records");
    expect(default_json.find("\"evidence_references\":3") != std::string::npos,
           "default JSON must count omitted visible evidence references");

    ltfa::ModelOutputContext explicit_context;
    explicit_context.restricted_policy = ltfa::RestrictedOutputPolicy::include_explicit;
    const auto explicit_json = ltfa::render_model_json(*model, explicit_context);
    const auto explicit_dot = ltfa::render_model_dot(*model, explicit_context);
    expect(explicit_json.find("\"id\":\"hidden\"") != std::string::npos &&
               explicit_json.find("evidence:hidden") != std::string::npos,
           "explicit policy must include restricted objects and evidence references");
    expect(explicit_dot.find("\"hidden\"") != std::string::npos,
           "explicit policy must include restricted DOT nodes");
    expect(explicit_dot.find("evidence:") == std::string::npos,
           "DOT must never place evidence references in graph labels");

    const auto restricted_space =
        compile("ltfa 1\n"
                "scope s\n"
                "entity private-space kind=network-space plane=communication layer=l3 scope=s "
                "sensitivity=restricted\n"
                "entity client kind=device plane=communication layer=l3 scope=s "
                "network-space=private-space address=192.0.2.1\n"
                "endpoint client:p owner=client kind=network network-space=private-space "
                "address=192.0.2.1\n");
    if (restricted_space.has_value()) {
        const auto default_space_json = ltfa::render_model_json(*restricted_space);
        expect(default_space_json.find("private-space") == std::string::npos &&
                   default_space_json.find("\"client\"") == std::string::npos &&
                   default_space_json.find("192.0.2.1") == std::string::npos,
               "restricted network-space filtering must prevent indirect identity leaks");
    }
}

void test_determinism_and_escaping() {
    auto first = compile(minimal_source());
    const auto reordered = compile(minimal_source(true));
    if (!first.has_value() || !reordered.has_value()) {
        return;
    }
    ltfa::ModelOutputContext left_context;
    left_context.model_inputs = {"b.ltfa", "a.ltfa"};
    left_context.observation_inputs = {"z.jsonl", "a.jsonl"};
    left_context.analysis_at = "2026-07-28T12:00:00+09:00";
    ltfa::ModelOutputContext right_context;
    right_context.model_inputs = {"a.ltfa", "b.ltfa"};
    right_context.observation_inputs = {"a.jsonl", "z.jsonl"};
    right_context.analysis_at = "2026-07-28T12:00:00+09:00";
    expect(ltfa::render_model_human(*first, left_context) ==
               ltfa::render_model_human(*reordered, right_context),
           "human output must not depend on declaration or input-list order");
    expect(ltfa::render_model_json(*first, left_context) ==
               ltfa::render_model_json(*reordered, right_context),
           "JSON output must not depend on declaration or input-list order");
    expect(ltfa::render_model_dot(*first, left_context) ==
               ltfa::render_model_dot(*reordered, right_context),
           "DOT output must not depend on declaration or input-list order");

    const auto label = ltfa::StringId{static_cast<std::uint32_t>(first->strings.size())};
    first->strings.emplace_back("Edge \"A\"\\path\nline");
    first->entities.front().label = label;
    const auto json = ltfa::render_model_json(*first);
    const auto dot = ltfa::render_model_dot(*first);
    const auto human = ltfa::render_model_human(*first);
    expect(json.find("Edge \\\"A\\\"\\\\path\\nline") != std::string::npos,
           "JSON must escape quotes, slashes, and line breaks");
    expect(dot.find("Edge \\\"A\\\"\\\\path\\nline") != std::string::npos,
           "DOT must escape quotes, slashes, and line breaks");
    expect(human.find("Edge \\\"A\\\"\\\\path\\nline") != std::string::npos,
           "human output must make slashes and line breaks unambiguous");
}

void test_dot_views() {
    const auto model = compile(restricted_source());
    if (!model.has_value()) {
        return;
    }
    const auto relation =
        std::find_if(model->relations.begin(), model->relations.end(), [&](const auto &candidate) {
            return model->external_id(candidate.from) == "peer:p" ||
                   model->external_id(candidate.to) == "peer:p";
        });
    expect(relation != model->relations.end(), "view fixture must contain a public relation");
    if (relation == model->relations.end()) {
        return;
    }

    ltfa::DotOutputOptions focused;
    focused.highlighted_relations = {relation->id};
    focused.grouping = ltfa::DotGrouping::layer;
    const auto focused_dot = ltfa::render_model_dot(*model, {}, focused);
    expect(focused_dot.find("cluster:layer:physical") != std::string::npos,
           "DOT must support layer grouping");
    expect(focused_dot.find("penwidth=\"3\"") != std::string::npos,
           "service-path or finding views must support stable highlights");
    expect(focused_dot.find("\"hidden\"") == std::string::npos,
           "focused views must retain default restricted filtering");

    ltfa::DotOutputOptions facility;
    facility.plane = ltfa::Plane::facility;
    const auto facility_dot = ltfa::render_model_dot(*model, {}, facility);
    expect(facility_dot.find("\"public\"") == std::string::npos,
           "plane-filtered DOT must omit nonmatching objects");

    ltfa::ModelOutputContext explicit_context;
    explicit_context.restricted_policy = ltfa::RestrictedOutputPolicy::include_explicit;
    ltfa::DotOutputOptions domain;
    domain.failure_domain = ltfa::FailureDomainId{0};
    domain.grouping = ltfa::DotGrouping::failure_domain;
    const auto domain_dot = ltfa::render_model_dot(*model, explicit_context, domain);
    expect(domain_dot.find("cluster:failure-domain:cabinet-secret") != std::string::npos,
           "DOT must support failure-domain filtering and grouping");
}

} // namespace

int main(int argc, char **argv) {
    if (argc == 2 && std::string_view{argv[1]} == "--emit-dot") {
        const auto model = compile(minimal_source());
        if (!model.has_value()) {
            return 1;
        }
        ltfa::ModelOutputContext context;
        context.model_inputs = {"z.ltfa", "a.ltfa", "a.ltfa"};
        std::cout << ltfa::render_model_dot(*model, context);
        return failures == 0 ? 0 : 1;
    }
    if (argc != 1) {
        std::cerr << "usage: ltfa_model_output_test [--emit-dot]\n";
        return 2;
    }

    test_golden_outputs();
    test_restricted_policy();
    test_determinism_and_escaping();
    test_dot_views();
    return failures == 0 ? 0 : 1;
}
