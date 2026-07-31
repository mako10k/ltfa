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
constexpr ltfa::StringId kFile{31};

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << message << '\n';
        ++failures;
    }
}

std::string record(std::string_view kind = "icmp", std::string_view outcome = "timeout",
                   std::string_view extra = {}) {
    return "{\"schema\":\"ltfa.observation/v1\","
           "\"probe_id\":\"probe:site:router:ping\","
           "\"target\":\"endpoint:site:router:lan\","
           "\"vantage\":\"node:cloud-gateway\","
           "\"kind\":\"" +
           std::string{kind} + "\",\"outcome\":\"" + std::string{outcome} +
           "\",\"observed_at\":\"2026-07-28T09:00:00+09:00\","
           "\"valid_for_ms\":60000" +
           std::string{extra} + "}";
}

std::optional<ltfa::Diagnostic>
decode_error(std::string_view input, ltfa::DiagnosticCode expected,
             const ltfa::ModelLimits &limits = ltfa::kDefaultModelLimits) {
    const auto result = ltfa::decode_observation_json_lines(input, kFile, limits);
    if (!result.observations.empty()) {
        std::cerr << "invalid observation input exposed partial records\n";
        ++failures;
    }
    if (!result.error.has_value()) {
        std::cerr << "expected observation diagnostic " << ltfa::to_string(expected) << '\n';
        ++failures;
        return std::nullopt;
    }
    if (result.error->code != expected) {
        std::cerr << "expected observation diagnostic " << ltfa::to_string(expected) << ", got "
                  << ltfa::to_string(result.error->code) << '\n';
        ++failures;
    }
    return result.error;
}

void test_complete_records() {
    const auto first = record("icmp", "timeout",
                              ",\"duration_ms\":1000,\"producer_sequence\":7,"
                              "\"producer_id\":\"monitor:cloud\","
                              "\"evidence_ref\":\"monitoring:event-123\","
                              "\"detail\":\"redacted summary\","
                              "\"x-zone\":\"東京\",\"x-active\":true");
    const auto second = record("metric", "pass", ",\"value\":42,\"unit\":\"ms\"");
    const auto third = record("discrete-state", "fail", ",\"state\":\"open\",\"x-null\":null");
    const auto input = first + "\r\n" + second + "\n" + third;

    const auto result = ltfa::decode_observation_json_lines(input, kFile);
    expect(result.ok(), "complete observation records must decode");
    expect(result.observations.size() == 3, "all observation records must be retained");
    if (result.observations.size() != 3) {
        return;
    }

    const auto &icmp = result.observations[0];
    expect(icmp.schema == "ltfa.observation/v1", "schema must be retained");
    expect(icmp.probe_id == "probe:site:router:ping", "probe ID must be retained");
    expect(icmp.target == "endpoint:site:router:lan", "target must be retained");
    expect(icmp.vantage == "node:cloud-gateway", "vantage must be retained");
    expect(icmp.kind == ltfa::ObservationKind::icmp, "ICMP kind must decode");
    expect(icmp.outcome == ltfa::ObservationOutcome::timeout, "timeout outcome must decode");
    expect(icmp.valid_for_ms == 60000, "validity must decode as an unsigned integer");
    expect(icmp.duration_ms == 1000, "duration must be retained");
    expect(icmp.producer_sequence == 7, "producer sequence must be retained");
    expect(icmp.producer_id == "monitor:cloud", "producer ID must be retained");
    expect(icmp.evidence_ref == "monitoring:event-123", "evidence reference must be retained");
    expect(icmp.detail == "redacted summary", "redacted detail must be retained");
    expect(icmp.observed_at.year == 2026 && icmp.observed_at.month == 7 &&
               icmp.observed_at.day == 28 && icmp.observed_at.hour == 9 &&
               icmp.observed_at.offset_minutes == 540,
           "timestamp components and offset must decode");
    expect(icmp.source.file == kFile && icmp.source.begin.line == 1 &&
               icmp.source.begin.column == 1,
           "record source must identify its input line");
    expect(icmp.extensions.size() == 2 && icmp.extensions[0].key == "x-active" &&
               icmp.extensions[1].key == "x-zone",
           "extensions must be sorted by key");
    if (icmp.extensions.size() == 2) {
        expect(std::get<bool>(icmp.extensions[0].value), "boolean extension must be typed");
        expect(std::get<std::string>(icmp.extensions[1].value) == "東京",
               "UTF-8 extension text must be retained");
    }

    const auto &metric = result.observations[1];
    expect(metric.kind == ltfa::ObservationKind::metric && metric.value.has_value() &&
               metric.unit == "ms",
           "metric value and unit must decode together");
    if (metric.value.has_value()) {
        expect(std::get<std::uint64_t>(*metric.value) == 42,
               "positive integer metric value must retain its integer type");
    }
    expect(metric.source.begin.line == 2 && metric.source.begin.byte_offset == first.size() + 2,
           "CRLF source offsets must account for both delimiter bytes");

    const auto &state = result.observations[2];
    expect(state.kind == ltfa::ObservationKind::discrete_state && state.state == "open",
           "discrete state must decode on the matching kind");
    expect(state.extensions.size() == 1 &&
               std::holds_alternative<std::monostate>(state.extensions[0].value),
           "null extension must retain an explicit null value");
}

void test_kind_and_outcome_enums() {
    const std::vector<std::pair<std::string_view, ltfa::ObservationKind>> kinds{
        {"icmp", ltfa::ObservationKind::icmp},
        {"tcp-connect", ltfa::ObservationKind::tcp_connect},
        {"custom-heartbeat", ltfa::ObservationKind::custom_heartbeat},
        {"metric", ltfa::ObservationKind::metric},
        {"discrete-state", ltfa::ObservationKind::discrete_state},
    };
    for (const auto &[text, expected] : kinds) {
        const auto result = ltfa::decode_observation_json_lines(record(text, "unknown"), kFile);
        expect(result.ok() && result.observations.size() == 1 &&
                   result.observations[0].kind == expected && ltfa::to_string(expected) == text,
               "every built-in observation kind must round trip");
    }

    const std::vector<std::pair<std::string_view, ltfa::ObservationOutcome>> outcomes{
        {"pass", ltfa::ObservationOutcome::pass},       {"fail", ltfa::ObservationOutcome::fail},
        {"timeout", ltfa::ObservationOutcome::timeout}, {"error", ltfa::ObservationOutcome::error},
        {"unknown", ltfa::ObservationOutcome::unknown},
    };
    for (const auto &[text, expected] : outcomes) {
        const auto result = ltfa::decode_observation_json_lines(record("icmp", text), kFile);
        expect(result.ok() && result.observations.size() == 1 &&
                   result.observations[0].outcome == expected && ltfa::to_string(expected) == text,
               "every observation outcome must round trip");
    }
}

void test_timestamp_profile() {
    const auto timestamp_record = [](std::string_view timestamp) {
        return "{\"schema\":\"ltfa.observation/v1\","
               "\"probe_id\":\"probe:p\",\"target\":\"endpoint:t\","
               "\"vantage\":\"node:v\",\"kind\":\"icmp\",\"outcome\":\"pass\","
               "\"observed_at\":\"" +
               std::string{timestamp} + "\",\"valid_for_ms\":0}";
    };

    const auto result = ltfa::decode_observation_json_lines(
        timestamp_record("2024-02-29t23:59:60.123456789z"), kFile);
    expect(result.ok() && result.observations[0].observed_at.second == 60 &&
               result.observations[0].observed_at.nanosecond == 123456789 &&
               result.observations[0].observed_at.fractional_digits == 9,
           "valid leap-day, leap-second, fractional timestamp must decode");

    for (const auto timestamp :
         {"2023-02-29T00:00:00Z", "2026-01-01T24:00:00Z", "2026-01-01T00:00:00",
          "2026-01-01T00:00:00-00:00", "2026-01-01T00:00:00.1234567890Z"}) {
        decode_error(timestamp_record(timestamp), ltfa::DiagnosticCode::observation_invalid_field);
    }
}

void test_malformed_json() {
    std::string invalid_utf8 = record().substr(0, record().size() - 1) + ",\"x-text\":\"";
    invalid_utf8.push_back(static_cast<char>(0xC0));
    invalid_utf8 += "\"}";
    const std::string bom = "\xEF\xBB\xBF" + record();

    const std::vector<std::string> cases{
        "[]",
        record().substr(0, record().size() - 1) + ",}",
        record().substr(0, record().size() - 1) + ",\"x-number\":NaN}",
        record().substr(0, record().size() - 1) + ",/*comment*/\"x-value\":1}",
        record() + record(),
        std::move(invalid_utf8),
        bom,
    };
    const std::vector<ltfa::DiagnosticCode> codes{
        ltfa::DiagnosticCode::observation_invalid,
        ltfa::DiagnosticCode::observation_invalid_json,
        ltfa::DiagnosticCode::observation_invalid_json,
        ltfa::DiagnosticCode::observation_invalid_json,
        ltfa::DiagnosticCode::observation_invalid_json,
        ltfa::DiagnosticCode::observation_invalid_json,
        ltfa::DiagnosticCode::observation_invalid_json,
    };
    for (std::size_t index = 0; index < cases.size(); ++index) {
        const auto first = decode_error(cases[index], codes[index]);
        const auto second = decode_error(cases[index], codes[index]);
        if (first.has_value() && second.has_value()) {
            expect(first->source.begin.line == second->source.begin.line &&
                       first->source.begin.column == second->source.begin.column &&
                       first->message == second->message,
                   "malformed JSON diagnostics must be deterministic");
        }
    }
}

void test_schema_validation() {
    decode_error("{}", ltfa::DiagnosticCode::observation_missing_field);
    decode_error("{\"schema\":\"ltfa.observation/v1\",\"schema\":\"ltfa.observation/v1\"}",
                 ltfa::DiagnosticCode::observation_duplicate_field);

    auto future = record();
    future.replace(future.find("ltfa.observation/v1"), std::string{"ltfa.observation/v1"}.size(),
                   "ltfa.observation/v2");
    decode_error(future, ltfa::DiagnosticCode::observation_unsupported_schema);

    decode_error(record("icmp", "pass", ",\"extra\":1"),
                 ltfa::DiagnosticCode::observation_unknown_field);
    decode_error(record("unsupported", "pass"), ltfa::DiagnosticCode::observation_invalid_field);
    decode_error(record("icmp", "unsupported"), ltfa::DiagnosticCode::observation_invalid_field);
    auto negative_validity = record();
    negative_validity.replace(negative_validity.find("60000"), 5, "-1");
    decode_error(negative_validity, ltfa::DiagnosticCode::observation_invalid_field);

    auto invalid_id = record();
    invalid_id.replace(invalid_id.find("probe:site:router:ping"),
                       std::string{"probe:site:router:ping"}.size(), "Probe:Upper");
    decode_error(invalid_id, ltfa::DiagnosticCode::observation_invalid_field);

    decode_error(record("metric", "pass", ",\"value\":1"),
                 ltfa::DiagnosticCode::observation_invalid_field);
    decode_error(record("icmp", "pass", ",\"value\":1,\"unit\":\"ms\""),
                 ltfa::DiagnosticCode::observation_invalid_field);
    decode_error(record("icmp", "pass", ",\"state\":\"open\""),
                 ltfa::DiagnosticCode::observation_invalid_field);
    decode_error(record("icmp", "pass", ",\"x-nested\":{}"),
                 ltfa::DiagnosticCode::observation_invalid_field);
    decode_error(record("icmp", "pass", ",\"x-Bad\":1"),
                 ltfa::DiagnosticCode::observation_invalid_field);
    decode_error(record("icmp", "pass", ",\"x-api-key\":\"reference\""),
                 ltfa::DiagnosticCode::observation_invalid_field);
    decode_error(record("icmp", "pass", ",\"detail\":\"line\\nfeed\""),
                 ltfa::DiagnosticCode::observation_invalid_field);
    decode_error(record("icmp", "pass", ",\"producer_id\":\"Monitor Upper\""),
                 ltfa::DiagnosticCode::observation_invalid_field);
}

void test_fail_closed_and_limits() {
    const auto valid = record();
    const auto error =
        decode_error(valid + "\n{}", ltfa::DiagnosticCode::observation_missing_field);
    expect(error.has_value() && error->source.begin.line == 2 &&
               error->source.begin.byte_offset == valid.size() + 1,
           "a later invalid line must fail the complete batch at its stable location");

    auto limits = ltfa::kDefaultModelLimits;
    limits.max_observation_input_bytes = valid.size();
    expect(ltfa::decode_observation_json_lines(valid, kFile, limits).ok(),
           "exact observation input byte limit must be accepted");
    limits.max_observation_input_bytes = valid.size() - 1;
    decode_error(valid, ltfa::DiagnosticCode::limit_observation_input_bytes, limits);

    limits = ltfa::kDefaultModelLimits;
    limits.max_observation_line_bytes = valid.size();
    expect(ltfa::decode_observation_json_lines(valid, kFile, limits).ok(),
           "exact observation line byte limit must be accepted");
    limits.max_observation_line_bytes = valid.size() - 1;
    decode_error(valid, ltfa::DiagnosticCode::limit_observation_line_bytes, limits);

    limits = ltfa::kDefaultModelLimits;
    limits.max_observations = 1;
    decode_error(valid + "\n" + valid, ltfa::DiagnosticCode::limit_observation_records, limits);

    const auto extensions = record("icmp", "pass", ",\"x-one\":1,\"x-two\":2");
    limits = ltfa::kDefaultModelLimits;
    limits.max_observation_extensions = 2;
    expect(ltfa::decode_observation_json_lines(extensions, kFile, limits).ok(),
           "exact observation extension limit must be accepted");
    limits.max_observation_extensions = 1;
    decode_error(extensions, ltfa::DiagnosticCode::limit_observation_extensions, limits);

    const std::string short_record =
        "{\"schema\":\"ltfa.observation/v1\",\"probe_id\":\"p\",\"target\":\"t\","
        "\"vantage\":\"v\",\"kind\":\"icmp\",\"outcome\":\"pass\","
        "\"observed_at\":\"2026-07-31T12:00:00Z\",\"valid_for_ms\":0,"
        "\"detail\":\"12345678901234567890\"}";
    limits = ltfa::kDefaultModelLimits;
    limits.max_observation_string_bytes = 20;
    expect(ltfa::decode_observation_json_lines(short_record, kFile, limits).ok(),
           "exact observation string byte limit must be accepted");
    limits.max_observation_string_bytes = 19;
    decode_error(short_record, ltfa::DiagnosticCode::observation_invalid_field, limits);

    const auto extension_key = std::string{"x-"} + std::string(62, 'a');
    const auto exact_key = record("icmp", "pass", ",\"" + extension_key + "\":1");
    expect(ltfa::decode_observation_json_lines(exact_key, kFile).ok(),
           "exact extension key byte limit must be accepted");
    const auto long_key = record("icmp", "pass", ",\"" + extension_key + "a\":1");
    decode_error(long_key, ltfa::DiagnosticCode::observation_invalid_field);

    expect(ltfa::decode_observation_json_lines({}, kFile).ok(),
           "an empty observation input must decode as zero records");
    expect(ltfa::decode_observation_json_lines(valid + "\n", kFile).ok(),
           "one final JSON Lines delimiter must not create an empty record");
    decode_error("\n", ltfa::DiagnosticCode::observation_invalid_json);
}

} // namespace

int main() {
    test_complete_records();
    test_kind_and_outcome_enums();
    test_timestamp_profile();
    test_malformed_json();
    test_schema_validation();
    test_fail_closed_and_limits();
    return failures == 0 ? 0 : 1;
}
