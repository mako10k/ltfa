#include "ltfa/observation.hpp"

#include <yyjson.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ltfa {
namespace {

constexpr std::string_view kObservationSchema = "ltfa.observation/v1";
constexpr std::size_t kMaxExtensionKeyBytes = 64;
constexpr std::array<std::string_view, 16> kKnownFields{
    "schema",       "probe_id",    "target", "vantage", "kind",  "outcome",           "observed_at",
    "valid_for_ms", "duration_ms", "value",  "unit",    "state", "producer_sequence", "producer_id",
    "evidence_ref", "detail",
};
constexpr std::array<std::string_view, 8> kRequiredFields{
    "schema", "probe_id", "target", "vantage", "kind", "outcome", "observed_at", "valid_for_ms",
};

struct JsonDocument {
    yyjson_doc *value{nullptr};

    JsonDocument() = default;
    explicit JsonDocument(yyjson_doc *document) : value(document) {}
    JsonDocument(const JsonDocument &) = delete;
    JsonDocument &operator=(const JsonDocument &) = delete;
    ~JsonDocument() { yyjson_doc_free(value); }
};

struct FieldValue {
    std::string name;
    yyjson_val *value{nullptr};
};

struct RecordResult {
    std::optional<Observation> observation;
    std::optional<Diagnostic> error;
};

[[nodiscard]] std::size_t utf8_character_count(std::string_view text,
                                               std::size_t byte_limit) noexcept {
    std::size_t count = 0;
    std::size_t offset = 0;
    const auto limit = std::min(byte_limit, text.size());
    while (offset < limit) {
        const auto byte = static_cast<unsigned char>(text[offset]);
        std::size_t width = 1;
        if (byte >= 0xC2U && byte <= 0xDFU) {
            width = 2;
        } else if (byte >= 0xE0U && byte <= 0xEFU) {
            width = 3;
        } else if (byte >= 0xF0U && byte <= 0xF4U) {
            width = 4;
        }
        offset += std::min(width, limit - offset);
        ++count;
    }
    return count;
}

[[nodiscard]] SourceSpan line_span(StringId file, std::uint32_t line_number,
                                   std::uint64_t line_offset, std::string_view line) noexcept {
    const auto columns = utf8_character_count(line, line.size());
    return {
        file,
        {line_offset, line_number, 1},
        {line_offset + line.size(), line_number, static_cast<std::uint32_t>(columns + 1)},
    };
}

[[nodiscard]] SourceSpan error_span(StringId file, std::uint32_t line_number,
                                    std::uint64_t line_offset, std::string_view line,
                                    std::size_t error_offset) noexcept {
    const auto begin = std::min(error_offset, line.size());
    const auto end = begin < line.size() ? begin + 1 : begin;
    return {
        file,
        {line_offset + begin, line_number,
         static_cast<std::uint32_t>(utf8_character_count(line, begin) + 1)},
        {line_offset + end, line_number,
         static_cast<std::uint32_t>(utf8_character_count(line, end) + 1)},
    };
}

[[nodiscard]] Diagnostic make_error(DiagnosticCode code, SourceSpan source, std::string message) {
    return {
        code, DiagnosticSeverity::error, source, std::nullopt, std::nullopt, std::move(message),
    };
}

[[nodiscard]] bool is_known_field(std::string_view name) noexcept {
    return std::find(kKnownFields.begin(), kKnownFields.end(), name) != kKnownFields.end();
}

[[nodiscard]] bool is_extension_key(std::string_view name) noexcept {
    if (name.size() < 3 || name.size() > kMaxExtensionKeyBytes || name[0] != 'x' ||
        name[1] != '-') {
        return false;
    }
    return std::all_of(name.begin() + 2, name.end(), [](char value) {
        return (value >= 'a' && value <= 'z') || (value >= '0' && value <= '9') || value == '-';
    });
}

[[nodiscard]] bool has_sensitive_extension_name(std::string_view name) noexcept {
    constexpr std::array<std::string_view, 8> terms{
        "secret", "credential", "password", "passwd", "token", "private-key", "psk", "api-key",
    };
    return std::any_of(terms.begin(), terms.end(),
                       [name](std::string_view term) { return name.find(term) != name.npos; });
}

[[nodiscard]] bool is_identifier(std::string_view value) noexcept {
    if (value.empty()) {
        return false;
    }
    const auto first = value.front();
    if (!((first >= 'a' && first <= 'z') || (first >= '0' && first <= '9'))) {
        return false;
    }
    return std::all_of(value.begin() + 1, value.end(), [](char character) {
        return (character >= 'a' && character <= 'z') || (character >= '0' && character <= '9') ||
               character == '.' || character == '_' || character == ':' || character == '-';
    });
}

[[nodiscard]] bool contains_control(std::string_view value) noexcept {
    return std::any_of(value.begin(), value.end(), [](char character) {
        const auto byte = static_cast<unsigned char>(character);
        return byte < 0x20U || byte == 0x7FU;
    });
}

[[nodiscard]] yyjson_val *find_field(const std::vector<FieldValue> &fields,
                                     std::string_view name) noexcept {
    const auto found = std::find_if(fields.begin(), fields.end(),
                                    [name](const FieldValue &field) { return field.name == name; });
    return found == fields.end() ? nullptr : found->value;
}

[[nodiscard]] std::optional<std::string> read_string(yyjson_val *value, std::string_view field,
                                                     const ModelLimits &limits, SourceSpan source,
                                                     std::optional<Diagnostic> &error) {
    if (!yyjson_is_str(value)) {
        error = make_error(DiagnosticCode::observation_invalid_field, source,
                           "observation field '" + std::string{field} + "' must be a string");
        return std::nullopt;
    }
    const auto length = yyjson_get_len(value);
    const std::string result{yyjson_get_str(value), length};
    if (result.empty()) {
        error = make_error(DiagnosticCode::observation_invalid_field, source,
                           "observation field '" + std::string{field} + "' must not be empty");
        return std::nullopt;
    }
    if (result.size() > limits.max_observation_string_bytes) {
        error = make_error(DiagnosticCode::observation_invalid_field, source,
                           "observation field '" + std::string{field} +
                               "' exceeds string byte limit of " +
                               std::to_string(limits.max_observation_string_bytes));
        return std::nullopt;
    }
    if (contains_control(result)) {
        error = make_error(DiagnosticCode::observation_invalid_field, source,
                           "observation field '" + std::string{field} +
                               "' must not contain control characters");
        return std::nullopt;
    }
    return result;
}

[[nodiscard]] std::optional<std::uint64_t> read_unsigned(yyjson_val *value, std::string_view field,
                                                         SourceSpan source,
                                                         std::optional<Diagnostic> &error) {
    if (!yyjson_is_uint(value)) {
        error = make_error(DiagnosticCode::observation_invalid_field, source,
                           "observation field '" + std::string{field} +
                               "' must be an unsigned integer");
        return std::nullopt;
    }
    return yyjson_get_uint(value);
}

[[nodiscard]] std::optional<ObservationNumber> read_number(yyjson_val *value,
                                                           std::string_view field,
                                                           SourceSpan source,
                                                           std::optional<Diagnostic> &error) {
    if (yyjson_is_uint(value)) {
        return ObservationNumber{yyjson_get_uint(value)};
    }
    if (yyjson_is_sint(value)) {
        return ObservationNumber{yyjson_get_sint(value)};
    }
    if (yyjson_is_real(value) && std::isfinite(yyjson_get_real(value))) {
        return ObservationNumber{yyjson_get_real(value)};
    }
    error = make_error(DiagnosticCode::observation_invalid_field, source,
                       "observation field '" + std::string{field} + "' must be a finite number");
    return std::nullopt;
}

[[nodiscard]] std::optional<ObservationExtensionValue>
read_extension(yyjson_val *value, std::string_view field, const ModelLimits &limits,
               SourceSpan source, std::optional<Diagnostic> &error) {
    if (yyjson_is_null(value)) {
        return ObservationExtensionValue{std::monostate{}};
    }
    if (yyjson_is_bool(value)) {
        return ObservationExtensionValue{yyjson_get_bool(value)};
    }
    if (yyjson_is_uint(value)) {
        return ObservationExtensionValue{yyjson_get_uint(value)};
    }
    if (yyjson_is_sint(value)) {
        return ObservationExtensionValue{yyjson_get_sint(value)};
    }
    if (yyjson_is_real(value) && std::isfinite(yyjson_get_real(value))) {
        return ObservationExtensionValue{yyjson_get_real(value)};
    }
    if (yyjson_is_str(value)) {
        const auto decoded = read_string(value, field, limits, source, error);
        if (decoded.has_value()) {
            return ObservationExtensionValue{*decoded};
        }
        return std::nullopt;
    }
    error = make_error(DiagnosticCode::observation_invalid_field, source,
                       "observation extension '" + std::string{field} +
                           "' must be null, boolean, number, or string");
    return std::nullopt;
}

[[nodiscard]] bool parse_two_digits(std::string_view value, std::size_t offset,
                                    std::uint8_t &output) noexcept {
    if (offset + 2 > value.size() || value[offset] < '0' || value[offset] > '9' ||
        value[offset + 1] < '0' || value[offset + 1] > '9') {
        return false;
    }
    output = static_cast<std::uint8_t>((value[offset] - '0') * 10 + value[offset + 1] - '0');
    return true;
}

[[nodiscard]] bool is_leap_year(std::uint16_t year) noexcept {
    return (year % 4U == 0U && year % 100U != 0U) || year % 400U == 0U;
}

[[nodiscard]] std::uint8_t days_in_month(std::uint16_t year, std::uint8_t month) noexcept {
    constexpr std::array<std::uint8_t, 12> days{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 0 || month > days.size()) {
        return 0;
    }
    if (month == 2 && is_leap_year(year)) {
        return 29;
    }
    return days[month - 1];
}

[[nodiscard]] std::optional<ObservationTimestamp>
parse_timestamp(std::string_view value, SourceSpan source, std::optional<Diagnostic> &error) {
    const auto fail = [&]() -> std::optional<ObservationTimestamp> {
        error = make_error(
            DiagnosticCode::observation_invalid_field, source,
            "observation field 'observed_at' must be an RFC 3339 timestamp with an offset");
        return std::nullopt;
    };

    if (value.size() < 20 || value[4] != '-' || value[7] != '-' ||
        (value[10] != 'T' && value[10] != 't') || value[13] != ':' || value[16] != ':') {
        return fail();
    }

    ObservationTimestamp timestamp;
    std::uint8_t year_high = 0;
    std::uint8_t year_low = 0;
    if (!parse_two_digits(value, 0, year_high) || !parse_two_digits(value, 2, year_low) ||
        !parse_two_digits(value, 5, timestamp.month) ||
        !parse_two_digits(value, 8, timestamp.day) ||
        !parse_two_digits(value, 11, timestamp.hour) ||
        !parse_two_digits(value, 14, timestamp.minute) ||
        !parse_two_digits(value, 17, timestamp.second)) {
        return fail();
    }
    timestamp.year = static_cast<std::uint16_t>(year_high * 100U + year_low);
    if (timestamp.year == 0 || timestamp.month == 0 || timestamp.month > 12 || timestamp.day == 0 ||
        timestamp.day > days_in_month(timestamp.year, timestamp.month) || timestamp.hour > 23 ||
        timestamp.minute > 59 || timestamp.second > 60) {
        return fail();
    }

    std::size_t offset = 19;
    if (offset < value.size() && value[offset] == '.') {
        ++offset;
        const auto fraction_begin = offset;
        while (offset < value.size() && value[offset] >= '0' && value[offset] <= '9') {
            if (offset - fraction_begin >= 9) {
                return fail();
            }
            timestamp.nanosecond =
                timestamp.nanosecond * 10U + static_cast<std::uint32_t>(value[offset] - '0');
            ++offset;
        }
        timestamp.fractional_digits = static_cast<std::uint8_t>(offset - fraction_begin);
        if (timestamp.fractional_digits == 0) {
            return fail();
        }
        for (std::uint8_t digit = timestamp.fractional_digits; digit < 9; ++digit) {
            timestamp.nanosecond *= 10U;
        }
    }

    if (offset == value.size()) {
        return fail();
    }
    if ((value[offset] == 'Z' || value[offset] == 'z') && offset + 1 == value.size()) {
        timestamp.offset_minutes = 0;
    } else {
        if ((value[offset] != '+' && value[offset] != '-') || offset + 6 != value.size() ||
            value[offset + 3] != ':') {
            return fail();
        }
        std::uint8_t offset_hour = 0;
        std::uint8_t offset_minute = 0;
        if (!parse_two_digits(value, offset + 1, offset_hour) ||
            !parse_two_digits(value, offset + 4, offset_minute) || offset_hour > 23 ||
            offset_minute > 59 ||
            (value[offset] == '-' && offset_hour == 0 && offset_minute == 0)) {
            return fail();
        }
        const auto minutes = static_cast<std::int16_t>(offset_hour * 60U + offset_minute);
        timestamp.offset_minutes =
            value[offset] == '-' ? static_cast<std::int16_t>(-minutes) : minutes;
    }
    timestamp.text = std::string{value};
    return timestamp;
}

[[nodiscard]] std::optional<ObservationKind> parse_kind(std::string_view value) noexcept {
    if (value == "icmp") {
        return ObservationKind::icmp;
    }
    if (value == "tcp-connect") {
        return ObservationKind::tcp_connect;
    }
    if (value == "custom-heartbeat") {
        return ObservationKind::custom_heartbeat;
    }
    if (value == "metric") {
        return ObservationKind::metric;
    }
    if (value == "discrete-state") {
        return ObservationKind::discrete_state;
    }
    return std::nullopt;
}

[[nodiscard]] std::optional<ObservationOutcome> parse_outcome(std::string_view value) noexcept {
    if (value == "pass") {
        return ObservationOutcome::pass;
    }
    if (value == "fail") {
        return ObservationOutcome::fail;
    }
    if (value == "timeout") {
        return ObservationOutcome::timeout;
    }
    if (value == "error") {
        return ObservationOutcome::error;
    }
    if (value == "unknown") {
        return ObservationOutcome::unknown;
    }
    return std::nullopt;
}

[[nodiscard]] RecordResult decode_record(std::string_view line, StringId file,
                                         std::uint32_t line_number, std::uint64_t line_offset,
                                         const ModelLimits &limits) {
    const auto source = line_span(file, line_number, line_offset, line);
    if (line.empty()) {
        return {std::nullopt, make_error(DiagnosticCode::observation_invalid_json, source,
                                         "observation line must contain one JSON object")};
    }

    yyjson_read_err read_error{};
    auto *document = yyjson_read_opts(const_cast<char *>(line.data()), line.size(),
                                      YYJSON_READ_NOFLAG, nullptr, &read_error);
    JsonDocument owner{document};
    if (document == nullptr) {
        const std::string detail =
            read_error.msg == nullptr ? "unknown reader error" : read_error.msg;
        return {std::nullopt,
                make_error(DiagnosticCode::observation_invalid_json,
                           error_span(file, line_number, line_offset, line, read_error.pos),
                           "invalid observation JSON: " + detail)};
    }

    auto *root = yyjson_doc_get_root(document);
    if (!yyjson_is_obj(root)) {
        return {std::nullopt, make_error(DiagnosticCode::observation_invalid, source,
                                         "observation record must be a JSON object")};
    }

    std::vector<FieldValue> fields;
    std::vector<FieldValue> extensions;
    fields.reserve(kKnownFields.size());
    extensions.reserve(
        std::min<std::size_t>(yyjson_obj_size(root), limits.max_observation_extensions));
    std::vector<std::string> seen;
    seen.reserve(yyjson_obj_size(root));
    std::optional<std::string> unknown_field;

    auto iterator = yyjson_obj_iter_with(root);
    while (auto *key_value = yyjson_obj_iter_next(&iterator)) {
        const std::string key{yyjson_get_str(key_value), yyjson_get_len(key_value)};
        if (std::find(seen.begin(), seen.end(), key) != seen.end()) {
            return {std::nullopt, make_error(DiagnosticCode::observation_duplicate_field, source,
                                             "observation field '" + key + "' is duplicated")};
        }
        seen.push_back(key);
        auto *value = yyjson_obj_iter_get_val(key_value);
        if (is_known_field(key)) {
            fields.push_back({key, value});
        } else if (key.size() >= 2 && key[0] == 'x' && key[1] == '-') {
            if (!is_extension_key(key)) {
                return {std::nullopt,
                        make_error(DiagnosticCode::observation_invalid_field, source,
                                   "observation extension key '" + key + "' is invalid")};
            }
            if (has_sensitive_extension_name(key)) {
                return {std::nullopt, make_error(DiagnosticCode::observation_invalid_field, source,
                                                 "observation extension key '" + key +
                                                     "' is reserved for secret-bearing data")};
            }
            if (extensions.size() >= limits.max_observation_extensions) {
                return {std::nullopt,
                        make_error(DiagnosticCode::limit_observation_extensions, source,
                                   "observation exceeds extension limit of " +
                                       std::to_string(limits.max_observation_extensions))};
            }
            extensions.push_back({key, value});
        } else if (!unknown_field.has_value()) {
            unknown_field = key;
        }
    }

    for (const auto field : kRequiredFields) {
        if (find_field(fields, field) == nullptr) {
            return {std::nullopt, make_error(DiagnosticCode::observation_missing_field, source,
                                             "observation is missing required field '" +
                                                 std::string{field} + "'")};
        }
    }

    std::optional<Diagnostic> error;
    const auto schema = read_string(find_field(fields, "schema"), "schema", limits, source, error);
    if (!schema.has_value()) {
        return {std::nullopt, std::move(error)};
    }
    if (*schema != kObservationSchema) {
        return {std::nullopt,
                make_error(DiagnosticCode::observation_unsupported_schema, source,
                           "unsupported observation schema '" + *schema + "'; expected '" +
                               std::string{kObservationSchema} + "'")};
    }
    if (unknown_field.has_value()) {
        return {std::nullopt, make_error(DiagnosticCode::observation_unknown_field, source,
                                         "observation field '" + *unknown_field +
                                             "' is not defined; extensions must use an x- key")};
    }

    Observation observation;
    observation.schema = *schema;
    const auto read_id = [&](std::string_view field) -> std::optional<std::string> {
        auto result = read_string(find_field(fields, field), field, limits, source, error);
        if (result.has_value() && !is_identifier(*result)) {
            error = make_error(DiagnosticCode::observation_invalid_field, source,
                               "observation field '" + std::string{field} +
                                   "' must match [a-z0-9][a-z0-9._:-]*");
            return std::nullopt;
        }
        return result;
    };

    auto probe_id = read_id("probe_id");
    auto target = read_id("target");
    auto vantage = read_id("vantage");
    if (!probe_id.has_value() || !target.has_value() || !vantage.has_value()) {
        return {std::nullopt, std::move(error)};
    }
    observation.probe_id = std::move(*probe_id);
    observation.target = std::move(*target);
    observation.vantage = std::move(*vantage);

    const auto kind_text = read_string(find_field(fields, "kind"), "kind", limits, source, error);
    if (!kind_text.has_value()) {
        return {std::nullopt, std::move(error)};
    }
    const auto kind = parse_kind(*kind_text);
    if (!kind.has_value()) {
        return {std::nullopt,
                make_error(DiagnosticCode::observation_invalid_field, source,
                           "observation field 'kind' has unsupported value '" + *kind_text + "'")};
    }
    observation.kind = *kind;

    const auto outcome_text =
        read_string(find_field(fields, "outcome"), "outcome", limits, source, error);
    if (!outcome_text.has_value()) {
        return {std::nullopt, std::move(error)};
    }
    const auto outcome = parse_outcome(*outcome_text);
    if (!outcome.has_value()) {
        return {std::nullopt, make_error(DiagnosticCode::observation_invalid_field, source,
                                         "observation field 'outcome' has unsupported value '" +
                                             *outcome_text + "'")};
    }
    observation.outcome = *outcome;

    const auto observed_at_text =
        read_string(find_field(fields, "observed_at"), "observed_at", limits, source, error);
    if (!observed_at_text.has_value()) {
        return {std::nullopt, std::move(error)};
    }
    auto observed_at = parse_timestamp(*observed_at_text, source, error);
    if (!observed_at.has_value()) {
        return {std::nullopt, std::move(error)};
    }
    observation.observed_at = std::move(*observed_at);

    const auto valid_for =
        read_unsigned(find_field(fields, "valid_for_ms"), "valid_for_ms", source, error);
    if (!valid_for.has_value()) {
        return {std::nullopt, std::move(error)};
    }
    observation.valid_for_ms = *valid_for;

    const auto read_optional_string = [&](std::string_view field,
                                          std::optional<std::string> &output) -> bool {
        auto *value = find_field(fields, field);
        if (value == nullptr) {
            return true;
        }
        auto decoded = read_string(value, field, limits, source, error);
        if (!decoded.has_value()) {
            return false;
        }
        output = std::move(*decoded);
        return true;
    };
    const auto read_optional_unsigned = [&](std::string_view field,
                                            std::optional<std::uint64_t> &output) -> bool {
        auto *value = find_field(fields, field);
        if (value == nullptr) {
            return true;
        }
        const auto decoded = read_unsigned(value, field, source, error);
        if (!decoded.has_value()) {
            return false;
        }
        output = *decoded;
        return true;
    };

    if (!read_optional_unsigned("duration_ms", observation.duration_ms) ||
        !read_optional_unsigned("producer_sequence", observation.producer_sequence) ||
        !read_optional_string("unit", observation.unit) ||
        !read_optional_string("state", observation.state) ||
        !read_optional_string("producer_id", observation.producer_id) ||
        !read_optional_string("evidence_ref", observation.evidence_ref) ||
        !read_optional_string("detail", observation.detail)) {
        return {std::nullopt, std::move(error)};
    }
    if (observation.producer_id.has_value() && !is_identifier(*observation.producer_id)) {
        return {std::nullopt, make_error(DiagnosticCode::observation_invalid_field, source,
                                         "observation field 'producer_id' must match "
                                         "[a-z0-9][a-z0-9._:-]*")};
    }

    if (auto *value = find_field(fields, "value"); value != nullptr) {
        auto decoded = read_number(value, "value", source, error);
        if (!decoded.has_value()) {
            return {std::nullopt, std::move(error)};
        }
        observation.value = std::move(*decoded);
    }
    if (observation.value.has_value() != observation.unit.has_value()) {
        return {std::nullopt,
                make_error(DiagnosticCode::observation_invalid_field, source,
                           "observation fields 'value' and 'unit' must be supplied together")};
    }
    if (observation.value.has_value() && observation.kind != ObservationKind::metric) {
        return {std::nullopt,
                make_error(DiagnosticCode::observation_invalid_field, source,
                           "observation fields 'value' and 'unit' require kind 'metric'")};
    }
    if (observation.state.has_value() && observation.kind != ObservationKind::discrete_state) {
        return {std::nullopt,
                make_error(DiagnosticCode::observation_invalid_field, source,
                           "observation field 'state' requires kind 'discrete-state'")};
    }

    observation.extensions.reserve(extensions.size());
    for (const auto &extension : extensions) {
        auto value = read_extension(extension.value, extension.name, limits, source, error);
        if (!value.has_value()) {
            return {std::nullopt, std::move(error)};
        }
        observation.extensions.push_back({extension.name, std::move(*value)});
    }
    std::sort(observation.extensions.begin(), observation.extensions.end(),
              [](const ObservationExtension &left, const ObservationExtension &right) {
                  return left.key < right.key;
              });
    observation.source = source;
    return {std::move(observation), std::nullopt};
}

} // namespace

std::string_view to_string(ObservationKind value) noexcept {
    switch (value) {
    case ObservationKind::icmp:
        return "icmp";
    case ObservationKind::tcp_connect:
        return "tcp-connect";
    case ObservationKind::custom_heartbeat:
        return "custom-heartbeat";
    case ObservationKind::metric:
        return "metric";
    case ObservationKind::discrete_state:
        return "discrete-state";
    }
    return "unknown";
}

std::string_view to_string(ObservationOutcome value) noexcept {
    switch (value) {
    case ObservationOutcome::pass:
        return "pass";
    case ObservationOutcome::fail:
        return "fail";
    case ObservationOutcome::timeout:
        return "timeout";
    case ObservationOutcome::error:
        return "error";
    case ObservationOutcome::unknown:
        return "unknown";
    }
    return "unknown";
}

ObservationDecodeResult decode_observation_json_lines(std::string_view input, StringId file,
                                                      const ModelLimits &limits) {
    if (input.size() > limits.max_observation_input_bytes) {
        const SourceSpan source{file, {0, 1, 1}, {0, 1, 1}};
        return {{},
                make_error(DiagnosticCode::limit_observation_input_bytes, source,
                           "observation input is " + std::to_string(input.size()) +
                               " bytes; limit is " +
                               std::to_string(limits.max_observation_input_bytes) + " bytes")};
    }

    ObservationDecodeResult result;
    std::size_t input_offset = 0;
    std::uint32_t line_number = 1;
    while (input_offset < input.size()) {
        const auto newline = input.find('\n', input_offset);
        const auto raw_end = newline == input.npos ? input.size() : newline;
        auto line = input.substr(input_offset, raw_end - input_offset);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        if (line.size() > limits.max_observation_line_bytes) {
            result.observations.clear();
            result.error = make_error(
                DiagnosticCode::limit_observation_line_bytes,
                line_span(file, line_number, input_offset, line),
                "observation line is " + std::to_string(line.size()) + " bytes; limit is " +
                    std::to_string(limits.max_observation_line_bytes) + " bytes");
            return result;
        }
        if (result.observations.size() >= limits.max_observations) {
            result.observations.clear();
            result.error = make_error(DiagnosticCode::limit_observation_records,
                                      line_span(file, line_number, input_offset, line),
                                      "observation input exceeds record limit of " +
                                          std::to_string(limits.max_observations));
            return result;
        }

        auto record = decode_record(line, file, line_number, input_offset, limits);
        if (!record.observation.has_value()) {
            result.observations.clear();
            result.error = std::move(record.error);
            return result;
        }
        result.observations.push_back(std::move(*record.observation));

        if (newline == input.npos) {
            break;
        }
        input_offset = newline + 1;
        ++line_number;
    }
    return result;
}

} // namespace ltfa
