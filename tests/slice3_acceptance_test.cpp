#include "ltfa/diagnosis.hpp"
#include "ltfa/diagnosis_output.hpp"
#include "ltfa/dsl/include_resolver.hpp"
#include "ltfa/dsl/parser.hpp"
#include "ltfa/model_compiler.hpp"
#include "ltfa/observation.hpp"

#include "yyjson.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifndef LTFA_REDACTED_FIXTURE_ROOT
#error "LTFA_REDACTED_FIXTURE_ROOT must name the redacted fixture directory"
#endif

namespace {

constexpr ltfa::StringId kModelFile{71};
constexpr ltfa::StringId kObservationFile{72};

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

std::string read_file(const std::filesystem::path &path) {
    std::ifstream stream{path, std::ios::binary};
    expect(stream.is_open(), "Slice 3 fixture file must open");
    return {std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
}

ltfa::NormalizedModel compile_source(std::string_view source) {
    auto parsed = ltfa::dsl::parse_document(source, kModelFile);
    if (parsed.error.has_value()) {
        std::cerr << ltfa::to_string(parsed.error->code) << ": " << parsed.error->message << '\n';
    }
    expect(parsed.ok(), "Slice 3 inline model must parse");
    ltfa::dsl::LoadedDocument loaded;
    loaded.statements = std::move(parsed.document->statements);
    loaded.total_source_bytes = source.size();
    auto compiled = ltfa::compile_model(loaded);
    if (compiled.error.has_value()) {
        std::cerr << ltfa::to_string(compiled.error->code) << ": " << compiled.error->message
                  << '\n';
    }
    expect(compiled.ok(), "Slice 3 inline model must compile");
    return std::move(*compiled.model);
}

ltfa::NormalizedModel compile_fixture(const std::filesystem::path &root) {
    const auto loaded = ltfa::dsl::load_document_tree(root, "main.ltfa");
    if (loaded.error.has_value()) {
        std::cerr << ltfa::to_string(loaded.error->code) << ": " << loaded.error->message << '\n';
    }
    expect(loaded.ok(), "Slice 3 redacted model must load");
    auto compiled = ltfa::compile_model(*loaded.document);
    if (compiled.error.has_value()) {
        std::cerr << ltfa::to_string(compiled.error->code) << ": " << compiled.error->message
                  << '\n';
    }
    expect(compiled.ok(), "Slice 3 redacted model must compile");
    return std::move(*compiled.model);
}

ltfa::ObservationTimestamp timestamp(std::string_view value) {
    auto parsed = ltfa::parse_observation_timestamp(value);
    expect(parsed.ok(), "Slice 3 analysis reference time must parse");
    return std::move(*parsed.timestamp);
}

struct Analysis {
    ltfa::EvidenceIndex evidence;
    ltfa::DiagnosisResult diagnosis;
};

Analysis analyze(const ltfa::NormalizedModel &model, std::vector<ltfa::Observation> observations,
                 std::string_view analysis_at) {
    auto indexed =
        ltfa::build_evidence_index(model, std::move(observations), timestamp(analysis_at));
    if (indexed.error.has_value()) {
        std::cerr << ltfa::to_string(indexed.error->code) << ": " << indexed.error->message << '\n';
    }
    expect(indexed.ok(), "Slice 3 observations must index");
    Analysis analysis;
    analysis.evidence = std::move(*indexed.index);
    analysis.diagnosis = ltfa::diagnose_observations(model, analysis.evidence);
    expect(analysis.diagnosis.status == ltfa::DiagnosisStatus::complete,
           "Slice 3 fixture diagnosis must complete");
    return analysis;
}

std::vector<ltfa::Observation> decode_fixture(const std::filesystem::path &root) {
    auto decoded = ltfa::decode_observation_json_lines(read_file(root / "observations.jsonl"),
                                                       kObservationFile);
    if (decoded.error.has_value()) {
        std::cerr << ltfa::to_string(decoded.error->code) << ": " << decoded.error->message << '\n';
    }
    expect(decoded.ok(), "Slice 3 redacted observations must decode");
    return std::move(decoded.observations);
}

void validate_json_contract(const std::string &json) {
    yyjson_doc *document = yyjson_read(json.data(), json.size(), YYJSON_READ_NOFLAG);
    expect(document != nullptr, "diagnosis JSON must be syntactically valid");
    yyjson_val *root = yyjson_doc_get_root(document);
    expect(yyjson_is_obj(root), "diagnosis JSON root must be an object");
    expect(std::string_view{yyjson_get_str(yyjson_obj_get(root, "schema"))} == "ltfa.diagnosis/v1",
           "diagnosis JSON must expose its stable schema");
    expect(std::string_view{yyjson_get_str(yyjson_obj_get(root, "operation"))} == "diagnose" &&
               std::string_view{yyjson_get_str(yyjson_obj_get(root, "status"))} == "complete",
           "diagnosis JSON must expose operation and result status");
    expect(std::string_view{yyjson_get_str(yyjson_obj_get(root, "analysis_at"))} ==
               "2030-01-01T00:10:30Z",
           "diagnosis JSON must emit the explicit analysis reference time");

    yyjson_val *evidence = yyjson_obj_get(root, "evidence");
    yyjson_val *findings = yyjson_obj_get(root, "findings");
    expect(yyjson_is_arr(evidence) && yyjson_arr_size(evidence) == 6,
           "diagnosis JSON must retain all six normalized observations");
    expect(yyjson_is_arr(findings) && yyjson_arr_size(findings) == 10,
           "diagnosis JSON must retain the complete candidate set");
    yyjson_val *first = yyjson_arr_get_first(findings);
    yyjson_val *hypothesis = yyjson_obj_get(first, "hypothesis");
    expect(std::string_view{yyjson_get_str(yyjson_obj_get(hypothesis, "id"))} ==
               "failure-mode:failure:remote-path",
           "diagnosis JSON must lead with the distinct best hypothesis");
    expect(yyjson_get_uint(yyjson_obj_get(first, "rank")) == 1 &&
               !yyjson_get_bool(yyjson_obj_get(first, "tied")),
           "diagnosis JSON must retain explicit rank and tie state");
    yyjson_doc_free(document);
}

ltfa::DiagnosisOutputContext output_context() {
    ltfa::DiagnosisOutputContext context;
    context.model_inputs = {"z/main.ltfa", "a/main.ltfa", "a/main.ltfa"};
    context.observation_inputs = {"z/observations.jsonl", "a/observations.jsonl",
                                  "a/observations.jsonl"};
    return context;
}

void test_normalized_outputs(const std::filesystem::path &root) {
    const auto model = compile_fixture(root);
    auto forward = decode_fixture(root);
    auto reversed = forward;
    std::reverse(reversed.begin(), reversed.end());
    const auto first = analyze(model, std::move(forward), "2030-01-01T00:10:30Z");
    const auto second = analyze(model, std::move(reversed), "2030-01-01T00:10:30Z");
    const auto context = output_context();

    const auto first_json =
        ltfa::render_diagnosis_json(model, first.evidence, first.diagnosis, context);
    const auto second_json =
        ltfa::render_diagnosis_json(model, second.evidence, second.diagnosis, context);
    const auto first_dot =
        ltfa::render_diagnosis_dot(model, first.evidence, first.diagnosis, context);
    const auto second_dot =
        ltfa::render_diagnosis_dot(model, second.evidence, second.diagnosis, context);

    expect(first_json == second_json,
           "normalized diagnosis JSON must ignore observation arrival order");
    expect(first_dot == second_dot,
           "normalized diagnosis DOT must ignore observation arrival order");
    expect(first_json ==
                   ltfa::render_diagnosis_json(model, first.evidence, first.diagnosis, context) &&
               first_dot ==
                   ltfa::render_diagnosis_dot(model, first.evidence, first.diagnosis, context),
           "repeated diagnosis rendering must be byte-stable");
    validate_json_contract(first_json);

    expect(first_json.find("\"model_inputs\":[\"a/main.ltfa\",\"z/main.ltfa\"]") !=
                   std::string::npos &&
               first_json.find("\"observation_inputs\":[\"a/observations.jsonl\","
                               "\"z/observations.jsonl\"]") != std::string::npos,
           "diagnosis JSON must sort and deduplicate input identities");
    expect(first_json.find("\"detail\":") == std::string::npos &&
               first_json.find("\"evidence_ref\":") == std::string::npos &&
               first_json.find("\"producer_id\":") == std::string::npos &&
               first_json.find("\"extensions\":") == std::string::npos,
           "diagnosis JSON must omit unrestricted observation detail");
    expect(first_dot.find("ltfa.diagnosis.dot/v1") != std::string::npos &&
               first_dot.find("failure-mode:failure:remote-path") != std::string::npos &&
               first_dot.find("across-vantages") != std::string::npos,
           "diagnosis DOT must explain the selected finding and its conflict");

    ltfa::DiagnosisBudget exhausted_budget;
    exhausted_budget.max_hypotheses = 0;
    const auto incomplete = ltfa::diagnose_observations(model, first.evidence, exhausted_budget);
    const auto incomplete_json =
        ltfa::render_diagnosis_json(model, first.evidence, incomplete, context);
    expect(incomplete.status == ltfa::DiagnosisStatus::incomplete &&
               incomplete_json.find("\"status\":\"incomplete\"") != std::string::npos &&
               incomplete_json.find("\"incomplete_reasons\":[\"hypothesis-budget\"]") !=
                   std::string::npos &&
               incomplete_json.find("\"generated_hypotheses\":0") != std::string::npos,
           "diagnosis JSON must distinguish bounded incompleteness and retain usage");
}

void test_restricted_output_policy() {
    const auto model =
        compile_source("ltfa 1\n"
                       "scope s kind=site completeness=partial evidence-quality=inferred\n"
                       "entity hidden kind=device plane=communication layer=physical scope=s "
                       "sensitivity=restricted\n"
                       "entity public kind=process plane=service layer=application scope=s\n"
                       "failure failure:hidden target=hidden mode=device-failure\n"
                       "probe probe:hidden target=hidden vantage=public kind=icmp valid-for=60s\n");
    const std::string record =
        "{\"schema\":\"ltfa.observation/v1\",\"probe_id\":\"probe:hidden\","
        "\"target\":\"hidden\",\"vantage\":\"public\",\"kind\":\"icmp\","
        "\"outcome\":\"timeout\",\"observed_at\":\"2030-01-01T00:10:00Z\","
        "\"valid_for_ms\":60000,\"producer_id\":\"producer-a\","
        "\"evidence_ref\":\"monitoring:event-1\",\"detail\":\"bounded redacted detail\"}";
    auto decoded = ltfa::decode_observation_json_lines(record, kObservationFile);
    expect(decoded.ok(), "restricted diagnosis observation must decode");
    const auto analysis = analyze(model, std::move(decoded.observations), "2030-01-01T00:10:30Z");

    const auto default_json =
        ltfa::render_diagnosis_json(model, analysis.evidence, analysis.diagnosis);
    const auto default_dot =
        ltfa::render_diagnosis_dot(model, analysis.evidence, analysis.diagnosis);
    expect(default_json.find("hidden") == std::string::npos &&
               default_dot.find("hidden") == std::string::npos,
           "default diagnosis output must omit restricted model and observation identities");
    expect(default_json.find("\"warnings\":[\"content omitted by output policy\"]") !=
                   std::string::npos &&
               default_json.find("\"observation_details\":1") != std::string::npos,
           "default diagnosis JSON must report policy omissions without disclosing content");

    ltfa::DiagnosisOutputContext explicit_context;
    explicit_context.restricted_policy = ltfa::RestrictedOutputPolicy::include_explicit;
    const auto explicit_json =
        ltfa::render_diagnosis_json(model, analysis.evidence, analysis.diagnosis, explicit_context);
    const auto explicit_dot =
        ltfa::render_diagnosis_dot(model, analysis.evidence, analysis.diagnosis, explicit_context);
    expect(explicit_json.find("probe:hidden") != std::string::npos &&
               explicit_dot.find("failure-mode:failure:hidden") != std::string::npos,
           "explicit policy must reveal registered restricted identities");
    for (const auto value : {"producer-a", "monitoring:event-1", "bounded redacted detail"}) {
        expect(explicit_json.find(value) == std::string::npos &&
                   explicit_dot.find(value) == std::string::npos,
               "diagnosis output must never render unrestricted observation detail");
    }
}

std::string fixture_dot(const std::filesystem::path &root) {
    const auto model = compile_fixture(root);
    const auto analysis = analyze(model, decode_fixture(root), "2030-01-01T00:10:30Z");
    return ltfa::render_diagnosis_dot(model, analysis.evidence, analysis.diagnosis,
                                      output_context());
}

} // namespace

int main(int argc, char **argv) {
    const std::filesystem::path root{LTFA_REDACTED_FIXTURE_ROOT};
    if (argc == 2 && std::string_view{argv[1]} == "--emit-dot") {
        std::cout << fixture_dot(root);
        return 0;
    }
    expect(argc == 1, "Slice 3 acceptance accepts only --emit-dot");
    test_normalized_outputs(root);
    test_restricted_output_policy();
    return 0;
}
