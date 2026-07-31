#include "ltfa/diagnosis.hpp"
#include "ltfa/dsl/parser.hpp"
#include "ltfa/model_compiler.hpp"
#include "ltfa/observation.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr ltfa::StringId kModelFile{51};
constexpr ltfa::StringId kObservationFile{52};

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
        "scope site:partial kind=site completeness=partial evidence-quality=inferred\n"
        "entity facility:power-a kind=facility plane=facility layer=physical "
        "scope=site:verified\n"
        "entity facility:outside kind=facility plane=facility layer=physical "
        "scope=site:verified\n"
        "entity service:target kind=service plane=service layer=application "
        "scope=site:verified\n"
        "entity service:healthy kind=service plane=service layer=application "
        "scope=site:verified\n"
        "entity process:site kind=process plane=service layer=application scope=site:partial\n"
        "entity process:cloud kind=process plane=service layer=application "
        "scope=site:verified\n"
        "require service:target by facility:power-a group=power mode=all\n"
        "failure failure:power-a target=facility:power-a mode=breaker-trip\n"
        "failure failure:target target=service:target mode=heartbeat-failed\n"
        "failure failure:site-path target=process:site mode=probe-path-loss\n"
        "failure failure:outside target=facility:outside mode=power-loss\n"
        "probe probe:site target=service:target vantage=process:site kind=icmp "
        "valid-for=60s\n"
        "probe probe:cloud target=service:target vantage=process:cloud kind=icmp "
        "valid-for=60s\n"
        "probe probe:healthy target=service:healthy vantage=process:cloud kind=icmp "
        "valid-for=60s\n";

    auto parsed = ltfa::dsl::parse_document(source, kModelFile);
    expect(parsed.ok(), "diagnosis fixture must parse");
    ltfa::dsl::LoadedDocument loaded;
    loaded.statements = std::move(parsed.document->statements);
    loaded.total_source_bytes = source.size();
    auto compiled = ltfa::compile_model(loaded);
    if (compiled.error.has_value()) {
        std::cerr << ltfa::to_string(compiled.error->code) << ": " << compiled.error->message
                  << '\n';
    }
    expect(compiled.ok(), "diagnosis fixture must compile");
    return std::move(*compiled.model);
}

ltfa::ObservationTimestamp timestamp(std::string_view value) {
    auto parsed = ltfa::parse_observation_timestamp(value);
    expect(parsed.ok(), "diagnosis timestamp must parse");
    return std::move(*parsed.timestamp);
}

std::string record(std::string_view probe, std::string_view target, std::string_view vantage,
                   std::string_view outcome, std::string_view observed_at) {
    return "{\"schema\":\"ltfa.observation/v1\",\"probe_id\":\"" + std::string{probe} +
           "\",\"target\":\"" + std::string{target} + "\",\"vantage\":\"" + std::string{vantage} +
           "\",\"kind\":\"icmp\",\"outcome\":\"" + std::string{outcome} + "\",\"observed_at\":\"" +
           std::string{observed_at} + "\",\"valid_for_ms\":60000}";
}

std::string site_record(std::string_view outcome, std::string_view observed_at) {
    return record("probe:site", "service:target", "process:site", outcome, observed_at);
}

std::string cloud_record(std::string_view outcome, std::string_view observed_at) {
    return record("probe:cloud", "service:target", "process:cloud", outcome, observed_at);
}

ltfa::EvidenceIndex build_evidence(const ltfa::NormalizedModel &model, std::string_view lines,
                                   std::string_view analysis_at) {
    auto decoded = ltfa::decode_observation_json_lines(lines, kObservationFile);
    expect(decoded.ok(), "diagnosis observations must decode");
    auto indexed =
        ltfa::build_evidence_index(model, std::move(decoded.observations), timestamp(analysis_at));
    expect(indexed.ok(), "diagnosis observations must index");
    return std::move(*indexed.index);
}

std::vector<std::string_view> object_ids(const ltfa::NormalizedModel &model,
                                         const std::vector<ltfa::ObjectRef> &objects) {
    std::vector<std::string_view> result;
    for (const auto object : objects) {
        result.push_back(model.external_id(object));
    }
    return result;
}

const ltfa::DiagnosisFinding *find_finding(const ltfa::DiagnosisResult &result,
                                           std::string_view stable_id) {
    const auto iterator = std::find_if(result.findings.begin(), result.findings.end(),
                                       [&](const ltfa::DiagnosisFinding &finding) {
                                           return finding.hypothesis.stable_id == stable_id;
                                       });
    return iterator == result.findings.end() ? nullptr : &*iterator;
}

bool has_reason(const ltfa::DiagnosisResult &result, ltfa::DiagnosisIncompleteReason reason) {
    return std::find(result.incomplete_reasons.begin(), result.incomplete_reasons.end(), reason) !=
           result.incomplete_reasons.end();
}

void test_causal_cone_and_vantage_ranking(const ltfa::NormalizedModel &model) {
    const auto site = site_record("timeout", "2026-07-31T09:00:00Z");
    const auto cloud = cloud_record("pass", "2026-07-31T09:00:00Z");
    const auto evidence = build_evidence(model, site + "\n" + cloud, "2026-07-31T09:00:30Z");
    const auto result = ltfa::diagnose_observations(model, evidence);

    expect(result.status == ltfa::DiagnosisStatus::complete,
           "bounded diagnosis must complete for the focused fixture");
    expect(object_ids(model, result.causal_cone) ==
               std::vector<std::string_view>{"facility:power-a", "process:site", "service:target"},
           "causal cone must contain only the failed probe vantage, target, and upstream provider");
    expect(result.findings.size() == 6,
           "each causal object and each declared mode in the cone must produce one hypothesis");
    expect(find_finding(result, "failure-mode:failure:outside") == nullptr &&
               find_finding(result, "object:facility:outside") == nullptr &&
               find_finding(result, "object:process:cloud") == nullptr,
           "objects outside the failed observation causal cone must not become hypotheses");

    const auto &best = result.findings.front();
    expect(best.hypothesis.stable_id == "failure-mode:failure:site-path" && best.rank == 1 &&
               !best.tied && best.contradictions == 0 && best.explained_non_pass == 1 &&
               best.unexplained_non_pass == 0,
           "vantage-specific path loss must outrank target-down hypotheses that contradict a pass");
    expect(best.hypothesis.kind == ltfa::DiagnosisHypothesisKind::declared_failure_mode &&
               object_ids(model, best.affected_objects) ==
                   std::vector<std::string_view>{"process:site"},
           "the best ICMP hypothesis must remain an explicit simulated hypothesis");
    expect(best.qualification.completeness == ltfa::Completeness::partial &&
               best.qualification.evidence_quality == ltfa::EvidenceQuality::inferred,
           "finding qualification must retain the affected scope limits");
    expect(best.evidence.size() == 2 && best.conflicting_evidence.size() == 2,
           "current evidence and both sides of a vantage conflict must remain attached");
    expect(best.evidence[0].assessment == ltfa::DiagnosisEvidenceAssessment::consistent &&
               best.evidence[1].assessment ==
                   ltfa::DiagnosisEvidenceAssessment::supports_hypothesis,
           "stable evidence order must expose a consistent pass and supporting timeout");

    const auto *target = find_finding(result, "failure-mode:failure:target");
    expect(target != nullptr && target->contradictions == 1 && target->explained_non_pass == 1,
           "a target-down hypothesis must retain the contradictory passing vantage");
}

void test_ties_and_input_order(const ltfa::NormalizedModel &model) {
    const auto site = site_record("timeout", "2026-07-31T09:00:00Z");
    const auto cloud = cloud_record("pass", "2026-07-31T09:00:00Z");
    const auto forward = ltfa::diagnose_observations(
        model, build_evidence(model, site + "\n" + cloud, "2026-07-31T09:00:30Z"));
    const auto reverse = ltfa::diagnose_observations(
        model, build_evidence(model, cloud + "\n" + site, "2026-07-31T09:00:30Z"));
    expect(forward.findings.size() == reverse.findings.size(),
           "observation arrival order must not change the candidate count");
    for (std::size_t index = 0; index < forward.findings.size(); ++index) {
        const auto &left = forward.findings[index];
        const auto &right = reverse.findings[index];
        expect(left.hypothesis.stable_id == right.hypothesis.stable_id && left.rank == right.rank &&
                   left.tied == right.tied && left.contradictions == right.contradictions &&
                   left.explained_non_pass == right.explained_non_pass &&
                   left.unexplained_non_pass == right.unexplained_non_pass,
               "identical semantic input must produce an identical ordered ranking");
    }

    const auto one_timeout =
        ltfa::diagnose_observations(model, build_evidence(model, site, "2026-07-31T09:00:30Z"));
    expect(one_timeout.findings.size() >= 2 &&
               one_timeout.findings[0].hypothesis.stable_id == "failure-mode:failure:power-a" &&
               one_timeout.findings[1].hypothesis.stable_id == "failure-mode:failure:target" &&
               one_timeout.findings[0].rank == 1 && one_timeout.findings[1].rank == 1 &&
               one_timeout.findings[0].tied && one_timeout.findings[1].tied,
           "equal score hypotheses must be reported as a tie and ordered by stable ID");

    const auto *generic_power = find_finding(one_timeout, "object:facility:power-a");
    expect(generic_power != nullptr && generic_power->rank > one_timeout.findings[0].rank,
           "a declared failure mode must outrank an otherwise equal generic object failure");
}

void test_stale_unknown_and_pass_only(const ltfa::NormalizedModel &model) {
    const auto stale = ltfa::diagnose_observations(
        model, build_evidence(model, site_record("timeout", "2026-07-31T09:00:00Z"),
                              "2026-07-31T09:02:00Z"));
    expect(
        stale.status == ltfa::DiagnosisStatus::complete && !stale.findings.empty() &&
            stale.findings[0].explained_non_pass == 0 &&
            stale.findings[0].unexplained_non_pass == 0 && stale.findings[0].stale_evidence == 1 &&
            stale.findings[0].remaining_unknown_evidence.size() == 1 &&
            stale.findings[0].evidence[0].assessment == ltfa::DiagnosisEvidenceAssessment::unknown,
        "stale observations must seed the cone without becoming current failure evidence");

    const auto pass = ltfa::diagnose_observations(
        model,
        build_evidence(model, site_record("pass", "2026-07-31T09:00:00Z"), "2026-07-31T09:00:30Z"));
    expect(pass.status == ltfa::DiagnosisStatus::complete && pass.causal_cone.empty() &&
               pass.findings.empty() && pass.usage.simulations == 0,
           "a fresh passing observation alone must not generate a failure hypothesis");
}

void test_structured_budgets_and_invalid_input(const ltfa::NormalizedModel &model) {
    const auto evidence = build_evidence(model,
                                         site_record("timeout", "2026-07-31T09:00:00Z") + "\n" +
                                             cloud_record("pass", "2026-07-31T09:00:00Z"),
                                         "2026-07-31T09:00:30Z");

    ltfa::DiagnosisBudget cone_budget;
    cone_budget.max_causal_objects = 1;
    const auto cone = ltfa::diagnose_observations(model, evidence, cone_budget);
    expect(cone.status == ltfa::DiagnosisStatus::incomplete && cone.causal_cone.size() == 1 &&
               has_reason(cone, ltfa::DiagnosisIncompleteReason::causal_cone_budget),
           "causal traversal exhaustion must return a bounded incomplete result");

    ltfa::DiagnosisBudget hypothesis_budget;
    hypothesis_budget.max_hypotheses = 1;
    const auto hypothesis = ltfa::diagnose_observations(model, evidence, hypothesis_budget);
    expect(hypothesis.status == ltfa::DiagnosisStatus::incomplete &&
               hypothesis.usage.candidate_hypotheses == 6 &&
               hypothesis.usage.generated_hypotheses == 1 && hypothesis.findings.size() == 1 &&
               has_reason(hypothesis, ltfa::DiagnosisIncompleteReason::hypothesis_budget),
           "hypothesis exhaustion must retain only the deterministic bounded prefix");

    ltfa::DiagnosisBudget simulation_budget;
    simulation_budget.max_simulations = 0;
    const auto simulation = ltfa::diagnose_observations(model, evidence, simulation_budget);
    expect(simulation.status == ltfa::DiagnosisStatus::incomplete && simulation.findings.empty() &&
               simulation.usage.simulations == 0 &&
               has_reason(simulation, ltfa::DiagnosisIncompleteReason::simulation_budget),
           "simulation exhaustion must be explicit without an uncomputed finding");

    ltfa::DiagnosisBudget comparison_budget;
    comparison_budget.max_evidence_comparisons = 1;
    const auto comparison = ltfa::diagnose_observations(model, evidence, comparison_budget);
    expect(comparison.status == ltfa::DiagnosisStatus::incomplete && comparison.findings.empty() &&
               comparison.usage.evidence_comparisons == 1 &&
               has_reason(comparison, ltfa::DiagnosisIncompleteReason::evidence_comparison_budget),
           "evidence comparison exhaustion must not expose a partially scored finding");

    ltfa::DiagnosisBudget propagation_budget;
    propagation_budget.propagation.max_events = 0;
    const auto propagation = ltfa::diagnose_observations(model, evidence, propagation_budget);
    expect(propagation.status == ltfa::DiagnosisStatus::incomplete &&
               propagation.findings.empty() && propagation.usage.simulations == 1 &&
               has_reason(propagation, ltfa::DiagnosisIncompleteReason::propagation_budget),
           "nested propagation exhaustion must remain a structured diagnosis limit");

    auto malformed = evidence;
    malformed.groups[0].target = {};
    expect(ltfa::diagnose_observations(model, malformed).status == ltfa::DiagnosisStatus::invalid,
           "a malformed evidence index must fail closed");
}

void test_enum_names() {
    expect(ltfa::to_string(ltfa::DiagnosisStatus::complete) == "complete" &&
               ltfa::to_string(ltfa::DiagnosisStatus::incomplete) == "incomplete" &&
               ltfa::to_string(ltfa::DiagnosisStatus::invalid) == "invalid" &&
               ltfa::to_string(ltfa::DiagnosisHypothesisKind::declared_failure_mode) ==
                   "declared-failure-mode" &&
               ltfa::to_string(ltfa::DiagnosisHypothesisKind::object_failure) == "object-failure" &&
               ltfa::to_string(ltfa::DiagnosisEvidenceAssessment::supports_hypothesis) ==
                   "supports-hypothesis" &&
               ltfa::to_string(ltfa::DiagnosisEvidenceAssessment::contradicts_hypothesis) ==
                   "contradicts-hypothesis" &&
               ltfa::to_string(ltfa::DiagnosisEvidenceAssessment::unexplained_non_pass) ==
                   "unexplained-non-pass" &&
               ltfa::to_string(ltfa::DiagnosisIncompleteReason::propagation_budget) ==
                   "propagation-budget",
           "diagnosis enum names must remain stable for later output contracts");
}

} // namespace

int main() {
    const auto model = compile_fixture();
    test_causal_cone_and_vantage_ranking(model);
    test_ties_and_input_order(model);
    test_stale_unknown_and_pass_only(model);
    test_structured_budgets_and_invalid_input(model);
    test_enum_names();
    return 0;
}
