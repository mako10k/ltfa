#include "ltfa/dsl/parser.hpp"
#include "ltfa/evidence.hpp"
#include "ltfa/model_compiler.hpp"
#include "ltfa/observation.hpp"

#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

int failures = 0;
constexpr ltfa::StringId kModelFile{41};
constexpr ltfa::StringId kObservationFile{42};

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << message << '\n';
        ++failures;
    }
}

std::optional<ltfa::NormalizedModel> compile_model(bool heartbeat_mapping = true) {
    std::string source =
        "ltfa 1\n"
        "scope site:test kind=site completeness=verified evidence-quality=confirmed\n"
        "entity device:target kind=device plane=communication layer=physical scope=site:test\n"
        "entity process:site kind=process plane=service layer=application scope=site:test\n"
        "entity process:cloud kind=process plane=service layer=application scope=site:test\n"
        "entity service:heartbeat kind=service plane=service layer=application scope=site:test\n"
        "probe probe:site target=device:target vantage=process:site kind=icmp valid-for=60s\n"
        "probe probe:cloud target=device:target vantage=process:cloud kind=icmp valid-for=60s\n";
    source += "probe probe:metric target=device:target vantage=process:site kind=metric "
              "valid-for=60s\n";
    source += "probe probe:heartbeat target=service:heartbeat vantage=process:site "
              "kind=custom-heartbeat valid-for=30s";
    if (heartbeat_mapping) {
        source += " x-outcome-map=common-outcomes-v1";
    }
    source += "\n";

    auto parsed = ltfa::dsl::parse_document(source, kModelFile);
    if (!parsed.ok()) {
        std::cerr << "evidence fixture parse failed";
        if (parsed.error.has_value()) {
            std::cerr << ": " << ltfa::to_string(parsed.error->code) << ": "
                      << parsed.error->message;
        }
        std::cerr << '\n';
        ++failures;
        return std::nullopt;
    }
    ltfa::dsl::LoadedDocument loaded;
    loaded.statements = std::move(parsed.document->statements);
    loaded.total_source_bytes = source.size();
    auto compiled = ltfa::compile_model(loaded);
    if (!compiled.ok()) {
        std::cerr << "evidence fixture compile failed";
        if (compiled.error.has_value()) {
            std::cerr << ": " << ltfa::to_string(compiled.error->code) << ": "
                      << compiled.error->message;
        }
        std::cerr << '\n';
        ++failures;
        return std::nullopt;
    }
    return std::move(compiled.model);
}

std::string record(std::string_view probe, std::string_view target, std::string_view vantage,
                   std::string_view kind, std::string_view outcome, std::string_view observed_at,
                   std::uint64_t valid_for_ms, std::string_view extra = {}) {
    return "{\"schema\":\"ltfa.observation/v1\",\"probe_id\":\"" + std::string{probe} +
           "\",\"target\":\"" + std::string{target} + "\",\"vantage\":\"" + std::string{vantage} +
           "\",\"kind\":\"" + std::string{kind} + "\",\"outcome\":\"" + std::string{outcome} +
           "\",\"observed_at\":\"" + std::string{observed_at} +
           "\",\"valid_for_ms\":" + std::to_string(valid_for_ms) + std::string{extra} + "}";
}

std::string site_record(std::string_view outcome, std::string_view observed_at,
                        std::string_view extra = {}) {
    return record("probe:site", "device:target", "process:site", "icmp", outcome, observed_at,
                  60'000, extra);
}

std::string cloud_record(std::string_view outcome, std::string_view observed_at,
                         std::string_view extra = {}) {
    return record("probe:cloud", "device:target", "process:cloud", "icmp", outcome, observed_at,
                  60'000, extra);
}

ltfa::ObservationTimestamp timestamp(std::string_view value) {
    auto parsed = ltfa::parse_observation_timestamp(value);
    if (!parsed.ok()) {
        std::cerr << "timestamp fixture failed to parse: " << value << '\n';
        ++failures;
        return {};
    }
    return std::move(*parsed.timestamp);
}

ltfa::EvidenceIndexResult build(const ltfa::NormalizedModel &model, std::string_view lines,
                                std::string_view analysis_at) {
    auto decoded = ltfa::decode_observation_json_lines(lines, kObservationFile);
    if (!decoded.ok()) {
        std::cerr << "observation fixture failed to decode";
        if (decoded.error.has_value()) {
            std::cerr << ": " << decoded.error->message;
        }
        std::cerr << '\n';
        ++failures;
        return {};
    }
    return ltfa::build_evidence_index(model, std::move(decoded.observations),
                                      timestamp(analysis_at));
}

std::optional<ltfa::Diagnostic> expect_error(const ltfa::NormalizedModel &model,
                                             std::string_view line, std::string_view analysis_at,
                                             ltfa::DiagnosticCode code) {
    auto result = build(model, line, analysis_at);
    expect(!result.index.has_value(), "failed evidence indexing must expose no partial index");
    if (!result.error.has_value()) {
        std::cerr << "expected evidence diagnostic " << ltfa::to_string(code) << '\n';
        ++failures;
        return std::nullopt;
    }
    if (result.error->code != code) {
        std::cerr << "expected evidence diagnostic " << ltfa::to_string(code) << ", got "
                  << ltfa::to_string(result.error->code) << ": " << result.error->message << '\n';
        ++failures;
    }
    return result.error;
}

void test_freshness_and_history(const ltfa::NormalizedModel &model) {
    const auto lines = site_record("pass", "2026-07-31T09:00:00+09:00") + "\n" +
                       site_record("timeout", "2026-07-31T09:01:00+09:00") + "\n" +
                       site_record("fail", "2026-07-31T08:59:00+09:00");
    auto result = build(model, lines, "2026-07-31T09:01:30+09:00");
    expect(result.ok(), "valid observations must build an evidence index");
    if (!result.index.has_value()) {
        return;
    }
    const auto &index = *result.index;
    expect(index.analysis_at.text == "2026-07-31T09:01:30+09:00",
           "explicit analysis_at must be retained");
    expect(index.groups.size() == 1 && index.groups[0].latest.count == 1 &&
               index.groups[0].history.count == 2,
           "newest observation must be selected and older records retained as history");
    expect(index.observations[index.groups[0].latest.begin].observation.outcome ==
               ltfa::ObservationOutcome::timeout,
           "newest outcome must be selected independent of input order");
    expect(index.observations[index.groups[0].latest.begin].freshness ==
               ltfa::EvidenceFreshness::current,
           "newest in-window observation must be current");
    for (std::uint32_t offset = 0; offset < index.groups[0].history.count; ++offset) {
        expect(index.observations[index.groups[0].history.begin + offset].freshness ==
                   ltfa::EvidenceFreshness::stale,
               "stale history must remain visible but not become current state");
    }
    expect(index.observations[0].meaning == ltfa::EvidenceMeaning::vantage_reachability,
           "ICMP evidence must remain scoped to reachability from its vantage");
    expect(index.conflicts.empty(), "ordered history alone must not create a conflict");

    auto exact = build(model, site_record("pass", "2026-07-31T09:00:00Z"), "2026-07-31T09:01:00Z");
    expect(exact.ok() && exact.index->observations[0].freshness == ltfa::EvidenceFreshness::current,
           "an observation must remain current at the exact validity boundary");
    auto after =
        build(model, site_record("pass", "2026-07-31T09:00:00Z"), "2026-07-31T09:01:00.000000001Z");
    expect(after.ok() && after.index->observations[0].freshness == ltfa::EvidenceFreshness::stale,
           "an observation must become stale immediately after its validity boundary");
}

void test_same_time_conflict(const ltfa::NormalizedModel &model) {
    const auto lines = site_record("timeout", "2026-07-31T09:01:00+09:00") + "\n" +
                       site_record("pass", "2026-07-31T00:01:00Z");
    const auto result = build(model, lines, "2026-07-31T09:01:30+09:00");
    expect(result.ok(), "same-time disagreement must be retained, not rejected");
    if (!result.index.has_value()) {
        return;
    }
    const auto &index = *result.index;
    expect(index.groups.size() == 1 && index.groups[0].latest.count == 2 &&
               index.groups[0].latest_conflicts,
           "equal instants with different offsets and outcomes must both remain latest");
    expect(index.conflicts.size() == 1 &&
               index.conflicts[0].kind == ltfa::EvidenceConflictKind::same_probe_and_vantage &&
               index.conflicts[0].observation_indices.size() == 2,
           "same-group outcome disagreement must produce an explicit conflict");
    expect(index.observations[0].conflicting && index.observations[1].conflicting,
           "every same-time disagreement record must be marked conflicting");

    const auto metric_one =
        record("probe:metric", "device:target", "process:site", "metric", "pass",
               "2026-07-31T09:01:00Z", 60'000, ",\"value\":1,\"unit\":\"ms\"");
    const auto metric_two =
        record("probe:metric", "device:target", "process:site", "metric", "pass",
               "2026-07-31T09:01:00Z", 60'000, ",\"value\":2,\"unit\":\"ms\"");
    const auto metric = build(model, metric_one + "\n" + metric_two, "2026-07-31T09:01:30Z");
    expect(metric.ok() && metric.index->groups[0].latest_conflicts &&
               metric.index->conflicts.size() == 1,
           "same-time metric value disagreement must be retained as a conflict");
}

void test_vantage_conflict_and_order(const ltfa::NormalizedModel &model) {
    const auto site = site_record("pass", "2026-07-31T09:01:00+09:00");
    const auto cloud = cloud_record("timeout", "2026-07-31T09:00:45+09:00");
    auto first = build(model, site + "\n" + cloud, "2026-07-31T09:01:30+09:00");
    auto second = build(model, cloud + "\n" + site, "2026-07-31T09:01:30+09:00");
    expect(first.ok() && second.ok(), "fresh observations from both vantages must index");
    if (!first.index.has_value() || !second.index.has_value()) {
        return;
    }
    expect(first.index->groups.size() == 2 && first.index->conflicts.size() == 1 &&
               first.index->conflicts[0].kind == ltfa::EvidenceConflictKind::across_vantages,
           "different fresh vantage outcomes must produce an across-vantages conflict");
    expect(first.index->conflicts[0].observation_indices.size() == 2,
           "both vantage observations must be retained in the conflict");
    expect(first.index->observations.size() == second.index->observations.size(),
           "arrival order must not change retained evidence count");
    for (std::size_t index = 0; index < first.index->observations.size(); ++index) {
        const auto &left = first.index->observations[index];
        const auto &right = second.index->observations[index];
        expect(left.observation.probe_id == right.observation.probe_id &&
                   left.observation.outcome == right.observation.outcome &&
                   left.freshness == right.freshness && left.conflicting == right.conflicting,
               "arrival order must not overwrite or reorder semantic evidence");
    }

    const auto stale_cloud = cloud_record("timeout", "2026-07-31T08:00:00+09:00");
    const auto stale = build(model, site + "\n" + stale_cloud, "2026-07-31T09:01:30+09:00");
    expect(stale.ok() && stale.index->observations.size() == 2 && stale.index->conflicts.empty(),
           "stale vantage evidence must remain visible without becoming current conflict state");
}

void test_probe_contract_and_reference_time(const ltfa::NormalizedModel &model) {
    const auto heartbeat = record("probe:heartbeat", "service:heartbeat", "process:site",
                                  "custom-heartbeat", "fail", "2026-07-31T09:01:00+09:00", 30'000);
    const auto heartbeat_result = build(model, heartbeat, "2026-07-31T09:01:10+09:00");
    expect(heartbeat_result.ok() && heartbeat_result.index->observations[0].meaning ==
                                        ltfa::EvidenceMeaning::registered_probe_outcome,
           "registered custom-heartbeat semantics must pass the complete probe contract");

    const auto missing_mapping_model = compile_model(false);
    expect(missing_mapping_model.has_value(),
           "a topology may compile before observation-specific heartbeat validation");
    if (missing_mapping_model.has_value()) {
        expect_error(*missing_mapping_model, heartbeat, "2026-07-31T09:01:10+09:00",
                     ltfa::DiagnosticCode::observation_contract_mismatch);
    }

    expect_error(model,
                 record("probe:missing", "device:target", "process:site", "icmp", "pass",
                        "2026-07-31T09:00:00Z", 60'000),
                 "2026-07-31T09:00:30Z", ltfa::DiagnosticCode::observation_probe_unresolved);
    expect_error(model,
                 record("probe:site", "device:other", "process:site", "icmp", "pass",
                        "2026-07-31T09:00:00Z", 60'000),
                 "2026-07-31T09:00:30Z", ltfa::DiagnosticCode::observation_contract_mismatch);
    expect_error(model,
                 record("probe:site", "device:target", "process:cloud", "icmp", "pass",
                        "2026-07-31T09:00:00Z", 60'000),
                 "2026-07-31T09:00:30Z", ltfa::DiagnosticCode::observation_contract_mismatch);
    expect_error(model,
                 record("probe:site", "device:target", "process:site", "tcp-connect", "pass",
                        "2026-07-31T09:00:00Z", 60'000),
                 "2026-07-31T09:00:30Z", ltfa::DiagnosticCode::observation_contract_mismatch);
    expect_error(model,
                 record("probe:site", "device:target", "process:site", "icmp", "pass",
                        "2026-07-31T09:00:00Z", 120'000),
                 "2026-07-31T09:00:30Z", ltfa::DiagnosticCode::observation_contract_mismatch);
    expect_error(model, site_record("pass", "2026-07-31T09:01:00Z"), "2026-07-31T09:00:59Z",
                 ltfa::DiagnosticCode::observation_after_reference_time);

    auto invalid_analysis = timestamp("2026-07-31T09:00:00Z");
    invalid_analysis.month = 13;
    const auto decoded = ltfa::decode_observation_json_lines(
        site_record("pass", "2026-07-31T09:00:00Z"), kObservationFile);
    auto invalid =
        ltfa::build_evidence_index(model, decoded.observations, std::move(invalid_analysis));
    expect(!invalid.index.has_value() && invalid.error.has_value() &&
               invalid.error->code == ltfa::DiagnosticCode::observation_invalid_reference_time,
           "invalid explicit analysis_at must fail before evidence classification");

    const auto empty = ltfa::build_evidence_index(model, {}, timestamp("2026-07-31T09:00:00Z"));
    expect(empty.ok() && empty.index->observations.empty() && empty.index->groups.empty(),
           "an explicit reference time with no observations must produce an empty index");

    auto invalid_model = model;
    for (auto &probe : invalid_model.probes) {
        if (invalid_model.string(probe.external_id) == "probe:heartbeat") {
            probe.id = ltfa::ProbeId{999};
        }
    }
    const auto invalid_contract = build(invalid_model, heartbeat, "2026-07-31T09:01:10+09:00");
    expect(!invalid_contract.index.has_value() && invalid_contract.error.has_value() &&
               invalid_contract.error->code == ltfa::DiagnosticCode::observation_contract_mismatch,
           "an inconsistent normalized probe ID must fail instead of indexing out of bounds");
}

void test_enum_names() {
    expect(ltfa::to_string(ltfa::EvidenceFreshness::current) == "current" &&
               ltfa::to_string(ltfa::EvidenceFreshness::stale) == "stale" &&
               ltfa::to_string(ltfa::EvidenceMeaning::registered_probe_outcome) ==
                   "registered-probe-outcome" &&
               ltfa::to_string(ltfa::EvidenceMeaning::vantage_reachability) ==
                   "vantage-reachability" &&
               ltfa::to_string(ltfa::EvidenceConflictKind::same_probe_and_vantage) ==
                   "same-probe-and-vantage" &&
               ltfa::to_string(ltfa::EvidenceConflictKind::across_vantages) == "across-vantages",
           "evidence enum names must remain stable for later output contracts");
}

} // namespace

int main() {
    auto model = compile_model();
    if (!model.has_value()) {
        return 1;
    }
    test_freshness_and_history(*model);
    test_same_time_conflict(*model);
    test_vantage_conflict_and_order(*model);
    test_probe_contract_and_reference_time(*model);
    test_enum_names();
    return failures == 0 ? 0 : 1;
}
