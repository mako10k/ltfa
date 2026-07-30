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

    ltfa::DotOutputOptions semantic;
    semantic.view = ltfa::DotView::topology;
    const auto semantic_dot = ltfa::render_model_dot(*first, {}, semantic);
    expect(semantic_dot.find("Edge \\\"A\\\"\\\\path\\nline") != std::string::npos,
           "semantic DOT must escape quotes, slashes, and line breaks");
}

std::string semantic_structure_source(bool reordered = false) {
    if (reordered) {
        return "ltfa 1\n"
               "connect router:lan -- switch:lan kind=network\n"
               "relate switch -> panel kind=contained_in\n"
               "endpoint switch:lan owner=switch kind=network label=\"Switch LAN\"\n"
               "entity switch kind=device plane=communication layer=physical scope=s "
               "label=\"Access Switch\"\n"
               "relate router -> panel kind=contained_in\n"
               "entity router kind=device plane=communication layer=physical scope=s "
               "label=\"Router\"\n"
               "endpoint router:lan owner=router kind=network "
               "label=\"Router <LAN> & edge\"\n"
               "relate panel -> facility kind=contained_in\n"
               "entity panel kind=facility plane=facility layer=physical scope=s "
               "label=\"Control Panel\"\n"
               "entity facility kind=facility plane=facility layer=physical scope=s "
               "label=\"Facility\"\n"
               "scope s kind=site label=\"Semantic Site\"\n";
    }
    return "ltfa 1\n"
           "scope s kind=site label=\"Semantic Site\"\n"
           "entity facility kind=facility plane=facility layer=physical scope=s "
           "label=\"Facility\"\n"
           "entity panel kind=facility plane=facility layer=physical scope=s "
           "label=\"Control Panel\"\n"
           "entity router kind=device plane=communication layer=physical scope=s label=\"Router\"\n"
           "entity switch kind=device plane=communication layer=physical scope=s "
           "label=\"Access Switch\"\n"
           "endpoint router:lan owner=router kind=network label=\"Router <LAN> & edge\"\n"
           "endpoint switch:lan owner=switch kind=network label=\"Switch LAN\"\n"
           "relate panel -> facility kind=contained_in\n"
           "relate router -> panel kind=contained_in\n"
           "relate switch -> panel kind=contained_in\n"
           "connect router:lan -- switch:lan kind=network\n";
}

void test_semantic_containment_clusters() {
    const auto model = compile(semantic_structure_source());
    const auto reordered = compile(semantic_structure_source(true));
    if (!model.has_value() || !reordered.has_value()) {
        return;
    }

    ltfa::DotOutputOptions options;
    options.view = ltfa::DotView::topology;
    const auto dot = ltfa::render_model_dot(*model, {}, options);
    expect(dot.find("ltfa_schema=\"ltfa.diagram.dot/v1\"") != std::string::npos &&
               dot.find("view=\"topology\"") != std::string::npos &&
               dot.find("rankdir=\"LR\"") != std::string::npos,
           "semantic DOT must identify its presentation schema, view, and stable orientation");
    const auto facility = dot.find("subgraph \"cluster:container:facility\"");
    const auto panel = dot.find("subgraph \"cluster:container:panel\"");
    const auto router = dot.find("\"router\" [label=");
    const auto access_switch = dot.find("\"switch\" [label=");
    expect(facility != std::string::npos && panel != std::string::npos &&
               router != std::string::npos && access_switch != std::string::npos &&
               facility < panel && panel < router && router < access_switch,
           "eligible containment must produce a stable nested cluster hierarchy");
    expect(dot.find("\"facility\" [label=") == std::string::npos &&
               dot.find("\"panel\" [label=") == std::string::npos,
           "folded containment parents must not also render as ordinary nodes");
    expect(dot.find("contained_in") == std::string::npos,
           "eligible containment must not remain as a semantic edge");
    expect(dot.find("label=\"network\"") == std::string::npos &&
               dot.find("ltfa_kind=\"network\"") != std::string::npos,
           "semantic relations must retain kind metadata without repeated visible labels");
    expect(dot.find("PORT=\"p0\" TOOLTIP=\"router:lan\"") != std::string::npos &&
               dot.find("PORT=\"p0\" TOOLTIP=\"switch:lan\"") != std::string::npos &&
               dot.find("\"router\":\"p0\" -> \"switch\":\"p0\"") != std::string::npos,
           "eligible endpoints must fold into stable owner ports used by relations");
    expect(dot.find("Router &lt;LAN&gt; &amp; edge") != std::string::npos,
           "semantic port labels must use deterministic HTML escaping");
    expect(dot.find("\"router:lan\" [label=") == std::string::npos &&
               dot.find("\"switch:lan\" [label=") == std::string::npos &&
               dot.find("label=\"owns\"") == std::string::npos,
           "folded semantic endpoints must omit endpoint nodes and synthetic owns edges");
    expect(ltfa::render_model_dot(*model, {}, options) ==
               ltfa::render_model_dot(*reordered, {}, options),
           "semantic containment output must not depend on declaration order");

    const auto restricted = compile(restricted_source());
    if (restricted.has_value()) {
        const auto restricted_dot = ltfa::render_model_dot(*restricted, {}, options);
        expect(restricted_dot.find("\"hidden\"") == std::string::npos,
               "semantic DOT must retain default restricted filtering");
    }
}

void test_semantic_containment_fallbacks() {
    const auto ambiguous =
        compile("ltfa 1\n"
                "scope s\n"
                "entity parent-a kind=facility plane=facility layer=physical scope=s\n"
                "entity parent-b kind=facility plane=facility layer=physical scope=s\n"
                "entity child kind=device plane=communication layer=physical scope=s\n"
                "relate child -> parent-a kind=contained_in\n"
                "relate child -> parent-b kind=contained_in\n");
    const auto cyclic =
        compile("ltfa 1\n"
                "scope s\n"
                "entity cycle-a kind=facility plane=facility layer=physical scope=s\n"
                "entity cycle-b kind=facility plane=facility layer=physical scope=s\n"
                "relate cycle-a -> cycle-b kind=contained_in\n"
                "relate cycle-b -> cycle-a kind=contained_in\n");
    const auto cross_scope =
        compile("ltfa 1\n"
                "scope local\n"
                "scope remote\n"
                "entity local-child kind=device plane=communication layer=physical scope=local\n"
                "entity remote-parent kind=facility plane=facility layer=physical scope=remote\n"
                "relate local-child -> remote-parent kind=contained_in\n");
    if (!ambiguous.has_value() || !cyclic.has_value() || !cross_scope.has_value()) {
        return;
    }

    ltfa::DotOutputOptions options;
    options.view = ltfa::DotView::topology;
    const auto ambiguous_dot = ltfa::render_model_dot(*ambiguous, {}, options);
    expect(ambiguous_dot.find("cluster:container:parent-a") == std::string::npos &&
               ambiguous_dot.find("cluster:container:parent-b") == std::string::npos,
           "multi-parent containment must not infer a spatial parent");
    const auto first_fallback = ambiguous_dot.find("label=\"contained_in\"");
    expect(first_fallback != std::string::npos &&
               ambiguous_dot.find("label=\"contained_in\"", first_fallback + 1) !=
                   std::string::npos,
           "multi-parent containment must retain every selected structural fact");

    const auto cyclic_dot = ltfa::render_model_dot(*cyclic, {}, options);
    expect(cyclic_dot.find("cluster:container:cycle-a") == std::string::npos &&
               cyclic_dot.find("cluster:container:cycle-b") == std::string::npos,
           "cyclic containment must not create nested clusters");
    expect(cyclic_dot.find("label=\"contained_in\"") != std::string::npos,
           "cyclic containment must remain an explicit structural edge");

    const auto cross_scope_dot = ltfa::render_model_dot(*cross_scope, {}, options);
    expect(cross_scope_dot.find("cluster:container:remote-parent") == std::string::npos &&
               cross_scope_dot.find("label=\"contained_in\"") != std::string::npos,
           "cross-scope containment must remain explicit instead of moving an object");

    const auto endpoint_conflict =
        compile("ltfa 1\n"
                "scope s\n"
                "entity owner kind=device plane=communication layer=physical scope=s\n"
                "entity other-parent kind=facility plane=facility layer=physical scope=s\n"
                "endpoint owner:net owner=owner kind=network label=\"Owner Network\"\n"
                "relate owner:net -> other-parent kind=contained_in\n");
    if (endpoint_conflict.has_value()) {
        const auto fallback_dot = ltfa::render_model_dot(*endpoint_conflict, {}, options);
        expect(fallback_dot.find("cluster:container:other-parent") != std::string::npos &&
                   fallback_dot.find("\"owner:net\" [label=\"Owner Network\"") !=
                       std::string::npos &&
                   fallback_dot.find("\"owner\" -> \"owner:net\" [label=\"owns\"") !=
                       std::string::npos,
               "endpoint placement that conflicts with its owner must retain node and owns edge");
    }
}

std::string relation_lens_source() {
    return "ltfa 1\n"
           "scope s label=\"Lens Site\"\n"
           "entity device-a kind=device plane=communication layer=physical scope=s\n"
           "entity device-b kind=device plane=communication layer=physical scope=s\n"
           "entity control-a kind=device plane=control-signal layer=physical scope=s\n"
           "entity control-b kind=device plane=control-signal layer=physical scope=s\n"
           "entity panel kind=facility plane=facility layer=physical scope=s\n"
           "entity container kind=facility plane=facility layer=physical scope=s\n"
           "entity contained-child kind=device plane=communication layer=physical scope=s\n"
           "entity overlay kind=tunnel plane=communication layer=overlay scope=s\n"
           "entity underlay kind=network-space plane=communication layer=l3 scope=s\n"
           "entity service kind=service plane=service layer=application scope=s\n"
           "entity group kind=service plane=service layer=application scope=s\n"
           "endpoint device-a:net owner=device-a kind=network\n"
           "endpoint device-b:net owner=device-b kind=network\n"
           "endpoint container:power owner=container kind=power-output\n"
           "endpoint device-a:power owner=device-a kind=power-input\n"
           "endpoint control-a:serial owner=control-a kind=serial\n"
           "endpoint control-b:serial owner=control-b kind=serial\n"
           "endpoint control-a:contact owner=control-a kind=contact-output\n"
           "endpoint control-b:contact owner=control-b kind=contact-input\n"
           "endpoint control-a:analog owner=control-a kind=analog-current-output\n"
           "endpoint control-b:analog owner=control-b kind=analog-current-input\n"
           "connect device-a:net -- device-b:net kind=network\n"
           "connect container:power -> device-a:power kind=power\n"
           "connect control-a:serial -- control-b:serial kind=serial\n"
           "connect control-a:contact -> control-b:contact kind=contact\n"
           "connect control-a:analog -> control-b:analog kind=analog-current\n"
           "relate service -> group kind=member_of\n"
           "relate contained-child -> container kind=contained_in\n"
           "relate overlay -> underlay kind=carried_by\n"
           "relate overlay -> underlay kind=uses_underlay\n"
           "relate overlay -> device-a kind=terminates_on\n"
           "relate service -> device-a kind=routes_via\n"
           "relate service -> device-b kind=depends_on\n"
           "relate device-a -> panel kind=powered_by\n";
}

bool has_semantic_relation_kind(std::string_view dot, std::string_view kind) {
    return dot.find("ltfa_kind=\"" + std::string{kind} + "\"") != std::string_view::npos;
}

void test_semantic_relation_lenses() {
    const auto model = compile(relation_lens_source());
    if (!model.has_value()) {
        return;
    }

    for (const auto view : {ltfa::DotView::raw, ltfa::DotView::topology, ltfa::DotView::power,
                            ltfa::DotView::dependency, ltfa::DotView::overlay}) {
        const auto parsed = ltfa::parse_dot_view(ltfa::to_string(view));
        expect(parsed.has_value() && *parsed == view,
               "every public DOT view name must round-trip exactly");
    }
    expect(!ltfa::parse_dot_view("Topology").has_value() &&
               !ltfa::parse_dot_view("unknown").has_value(),
           "unknown or differently-cased DOT views must be rejected");

    ltfa::DotOutputOptions options;
    options.view = ltfa::DotView::topology;
    const auto topology = ltfa::render_model_dot(*model, {}, options);
    expect(has_semantic_relation_kind(topology, "network") &&
               has_semantic_relation_kind(topology, "power") &&
               has_semantic_relation_kind(topology, "serial") &&
               has_semantic_relation_kind(topology, "contact") &&
               has_semantic_relation_kind(topology, "analog-current") &&
               has_semantic_relation_kind(topology, "member_of"),
           "topology view must include every physical and membership relation kind");
    expect(!has_semantic_relation_kind(topology, "powered_by") &&
               !has_semantic_relation_kind(topology, "depends_on") &&
               !has_semantic_relation_kind(topology, "uses_underlay"),
           "topology view must exclude power dependency and overlay relations");

    options.view = ltfa::DotView::power;
    const auto power = ltfa::render_model_dot(*model, {}, options);
    expect(has_semantic_relation_kind(power, "power") &&
               has_semantic_relation_kind(power, "powered_by"),
           "power view must include physical power and powered_by");
    expect(power.find("\"@ltfa-anchor:container\" [label=<<TABLE") != std::string::npos &&
               power.find("\"@ltfa-anchor:container\":\"p0\" -> \"device-a\":\"p1\"") !=
                   std::string::npos &&
               power.find("TOOLTIP=\"container:power\">power</TD>") != std::string::npos,
           "folded container endpoints must use stable ports on the internal cluster anchor");
    expect(!has_semantic_relation_kind(power, "network") &&
               !has_semantic_relation_kind(power, "depends_on"),
           "power view must exclude unrelated physical and dependency relations");

    options.view = ltfa::DotView::dependency;
    const auto dependency = ltfa::render_model_dot(*model, {}, options);
    expect(has_semantic_relation_kind(dependency, "depends_on") &&
               !has_semantic_relation_kind(dependency, "powered_by") &&
               !has_semantic_relation_kind(dependency, "routes_via"),
           "dependency view must contain only dependency edges plus spatial structure");

    options.view = ltfa::DotView::overlay;
    const auto overlay = ltfa::render_model_dot(*model, {}, options);
    expect(has_semantic_relation_kind(overlay, "carried_by") &&
               has_semantic_relation_kind(overlay, "uses_underlay") &&
               has_semantic_relation_kind(overlay, "terminates_on") &&
               has_semantic_relation_kind(overlay, "routes_via"),
           "overlay view must include all four overlay relation kinds");
    expect(!has_semantic_relation_kind(overlay, "network") &&
               !has_semantic_relation_kind(overlay, "depends_on"),
           "overlay view must exclude topology and dependency edges");

    const auto highlighted =
        std::find_if(model->relations.begin(), model->relations.end(), [](const auto &relation) {
            return relation.kind == ltfa::RelationKind::depends_on;
        });
    expect(highlighted != model->relations.end(),
           "relation lens fixture must include a dependency relation");
    if (highlighted != model->relations.end()) {
        options.view = ltfa::DotView::dependency;
        options.highlighted_relations = {highlighted->id};
        const auto highlighted_dot = ltfa::render_model_dot(*model, {}, options);
        expect(highlighted_dot.find("penwidth=\"3\"") != std::string::npos,
               "semantic relation lenses must preserve stable relation emphasis");
    }
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
    if (argc == 2) {
        const auto argument = std::string_view{argv[1]};
        std::optional<ltfa::DotView> emitted_view;
        if (argument == "--emit-dot") {
            emitted_view = ltfa::DotView::raw;
        } else if (argument == "--emit-semantic-dot") {
            emitted_view = ltfa::DotView::topology;
        } else if (argument == "--emit-semantic-power-dot") {
            emitted_view = ltfa::DotView::power;
        } else if (argument == "--emit-semantic-dependency-dot") {
            emitted_view = ltfa::DotView::dependency;
        } else if (argument == "--emit-semantic-overlay-dot") {
            emitted_view = ltfa::DotView::overlay;
        }
        if (!emitted_view.has_value()) {
            std::cerr << "usage: ltfa_model_output_test "
                         "[--emit-dot|--emit-semantic-dot|--emit-semantic-power-dot|"
                         "--emit-semantic-dependency-dot|--emit-semantic-overlay-dot]\n";
            return 2;
        }
        const bool relation_lens = *emitted_view == ltfa::DotView::power ||
                                   *emitted_view == ltfa::DotView::dependency ||
                                   *emitted_view == ltfa::DotView::overlay;
        const auto model =
            compile(relation_lens                              ? relation_lens_source()
                    : *emitted_view == ltfa::DotView::topology ? semantic_structure_source()
                                                               : minimal_source());
        if (!model.has_value()) {
            return 1;
        }
        ltfa::ModelOutputContext context;
        context.model_inputs = {"z.ltfa", "a.ltfa", "a.ltfa"};
        ltfa::DotOutputOptions options;
        options.view = *emitted_view;
        std::cout << ltfa::render_model_dot(*model, context, options);
        return failures == 0 ? 0 : 1;
    }
    if (argc != 1) {
        std::cerr << "usage: ltfa_model_output_test [DOT fixture option]\n";
        return 2;
    }

    test_golden_outputs();
    test_restricted_policy();
    test_determinism_and_escaping();
    test_semantic_containment_clusters();
    test_semantic_containment_fallbacks();
    test_semantic_relation_lenses();
    test_dot_views();
    return failures == 0 ? 0 : 1;
}
