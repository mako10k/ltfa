#include "ltfa/dsl/parser.hpp"
#include "ltfa/model_compiler.hpp"
#include "ltfa/observation.hpp"
#include "ltfa/observation_recommendation.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

namespace {

constexpr ltfa::StringId kModelFile{61};
constexpr ltfa::StringId kObservationFile{62};

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

ltfa::NormalizedModel compile_fixture() {
    const std::string source =
        "ltfa 1\n"
        "scope site:verified kind=site completeness=verified evidence-quality=confirmed\n"
        "entity facility:power kind=facility plane=facility layer=physical "
        "scope=site:verified\n"
        "entity service:target kind=service plane=service layer=application "
        "scope=site:verified\n"
        "entity service:healthy kind=service plane=service layer=application "
        "scope=site:verified\n"
        "entity process:site kind=process plane=service layer=application "
        "scope=site:verified\n"
        "entity process:cloud kind=process plane=service layer=application "
        "scope=site:verified\n"
        "require service:target by facility:power group=power mode=all\n"
        "failure failure:power target=facility:power mode=breaker-trip\n"
        "failure failure:target target=service:target mode=heartbeat-failed\n"
        "probe probe:site target=service:target vantage=process:site kind=icmp valid-for=60s\n"
        "probe probe:cloud target=service:target vantage=process:cloud kind=icmp valid-for=60s\n"
        "probe probe:power target=facility:power vantage=process:cloud kind=icmp valid-for=60s\n"
        "probe probe:healthy target=service:healthy vantage=process:cloud kind=icmp "
        "valid-for=60s\n";
    auto parsed = ltfa::dsl::parse_document(source, kModelFile);
    expect(parsed.ok(), "recommendation fixture must parse");
    ltfa::dsl::LoadedDocument loaded;
    loaded.statements = std::move(parsed.document->statements);
    loaded.total_source_bytes = source.size();
    auto compiled = ltfa::compile_model(loaded);
    expect(compiled.ok(), "recommendation fixture must compile");
    return std::move(*compiled.model);
}

ltfa::ObservationTimestamp timestamp(std::string_view value) {
    auto parsed = ltfa::parse_observation_timestamp(value);
    expect(parsed.ok(), "recommendation timestamp must parse");
    return std::move(*parsed.timestamp);
}

ltfa::EvidenceIndex build_evidence(const ltfa::NormalizedModel &model) {
    const std::string record = "{\"schema\":\"ltfa.observation/v1\",\"probe_id\":\"probe:site\","
                               "\"target\":\"service:target\",\"vantage\":\"process:site\","
                               "\"kind\":\"icmp\",\"outcome\":\"timeout\","
                               "\"observed_at\":\"2026-08-19T00:00:00Z\",\"valid_for_ms\":60000}";
    auto decoded = ltfa::decode_observation_json_lines(record, kObservationFile);
    expect(decoded.ok(), "recommendation observation must decode");
    auto indexed = ltfa::build_evidence_index(model, std::move(decoded.observations),
                                              timestamp("2026-08-19T00:00:30Z"));
    expect(indexed.ok(), "recommendation evidence must index");
    return std::move(*indexed.index);
}

std::string_view probe_id(const ltfa::NormalizedModel &model, ltfa::ProbeId id) {
    return model.string(model.probes[id.value()].external_id);
}

void test_recommendation(const ltfa::NormalizedModel &model) {
    const auto evidence = build_evidence(model);
    const auto diagnosis = ltfa::diagnose_observations(model, evidence);
    expect(diagnosis.status == ltfa::DiagnosisStatus::complete && diagnosis.findings.size() >= 2 &&
               diagnosis.findings[0].rank == 1 && diagnosis.findings[0].tied,
           "one timeout must retain a real top-ranked tie");
    const auto result = ltfa::recommend_next_observations(model, evidence, diagnosis);
    expect(result.status == ltfa::RecommendationStatus::complete && !result.recommendations.empty(),
           "a discriminating absent probe must produce a recommendation");
    const auto &recommendation = result.recommendations.front();
    expect(recommendation.diagnosis_rank == 1 &&
               probe_id(model, recommendation.probe) == "probe:power" &&
               recommendation.separated_pairs > 0,
           "the power probe must deterministically distinguish power and target hypotheses");
    expect(result.usage.candidate_probes > 0 && result.usage.hypothesis_comparisons > 0,
           "recommendation work must be explicit");

    const auto repeated = ltfa::recommend_next_observations(model, evidence, diagnosis);
    expect(repeated.status == result.status &&
               repeated.usage.candidate_probes == result.usage.candidate_probes &&
               repeated.usage.hypothesis_comparisons == result.usage.hypothesis_comparisons &&
               repeated.recommendations.size() == result.recommendations.size() &&
               repeated.recommendations.front().probe == recommendation.probe,
           "recommendation and usage must be deterministic");
}

void test_boundaries(const ltfa::NormalizedModel &model) {
    const auto evidence = build_evidence(model);
    const auto diagnosis = ltfa::diagnose_observations(model, evidence);

    ltfa::ObservationRecommendationBudget no_candidates;
    no_candidates.max_candidate_probes = 0;
    expect(ltfa::recommend_next_observations(model, evidence, diagnosis, no_candidates).status ==
               ltfa::RecommendationStatus::incomplete,
           "candidate budget exhaustion must be incomplete");

    ltfa::ObservationRecommendationBudget no_comparisons;
    no_comparisons.max_hypothesis_comparisons = 0;
    expect(ltfa::recommend_next_observations(model, evidence, diagnosis, no_comparisons).status ==
               ltfa::RecommendationStatus::incomplete,
           "comparison budget exhaustion must be incomplete");

    auto incomplete = diagnosis;
    incomplete.status = ltfa::DiagnosisStatus::incomplete;
    const auto from_incomplete = ltfa::recommend_next_observations(model, evidence, incomplete);
    expect(from_incomplete.status == ltfa::RecommendationStatus::incomplete &&
               from_incomplete.recommendations.empty(),
           "incomplete diagnosis must not produce a complete recommendation claim");

    auto invalid_model = model;
    invalid_model.probes.front().id = ltfa::ProbeId{999};
    expect(ltfa::recommend_next_observations(invalid_model, evidence, diagnosis).status ==
               ltfa::RecommendationStatus::invalid,
           "malformed registered probes must fail closed");

    auto invalid_evidence = evidence;
    invalid_evidence.groups.front().probe = ltfa::ProbeId{999};
    expect(ltfa::recommend_next_observations(model, invalid_evidence, diagnosis).status ==
               ltfa::RecommendationStatus::invalid,
           "malformed evidence groups must fail closed");
}

} // namespace

int main() {
    const auto model = compile_fixture();
    test_recommendation(model);
    test_boundaries(model);
    return 0;
}
