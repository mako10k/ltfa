#pragma once

#include "ltfa/error.hpp"
#include "ltfa/limits.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace ltfa {

enum class ObservationKind {
    icmp,
    tcp_connect,
    custom_heartbeat,
    metric,
    discrete_state,
};

enum class ObservationOutcome {
    pass,
    fail,
    timeout,
    error,
    unknown,
};

[[nodiscard]] std::string_view to_string(ObservationKind value) noexcept;
[[nodiscard]] std::string_view to_string(ObservationOutcome value) noexcept;

struct ObservationTimestamp {
    std::string text;
    std::uint16_t year{0};
    std::uint8_t month{0};
    std::uint8_t day{0};
    std::uint8_t hour{0};
    std::uint8_t minute{0};
    std::uint8_t second{0};
    std::uint32_t nanosecond{0};
    std::uint8_t fractional_digits{0};
    std::int16_t offset_minutes{0};
};

using ObservationNumber = std::variant<std::int64_t, std::uint64_t, double>;
using ObservationExtensionValue =
    std::variant<std::monostate, bool, std::int64_t, std::uint64_t, double, std::string>;

struct ObservationExtension {
    std::string key;
    ObservationExtensionValue value;
};

struct Observation {
    std::string schema;
    std::string probe_id;
    std::string target;
    std::string vantage;
    ObservationKind kind{ObservationKind::icmp};
    ObservationOutcome outcome{ObservationOutcome::unknown};
    ObservationTimestamp observed_at;
    std::uint64_t valid_for_ms{0};
    std::optional<std::uint64_t> duration_ms;
    std::optional<ObservationNumber> value;
    std::optional<std::string> unit;
    std::optional<std::string> state;
    std::optional<std::uint64_t> producer_sequence;
    std::optional<std::string> producer_id;
    std::optional<std::string> evidence_ref;
    std::optional<std::string> detail;
    std::vector<ObservationExtension> extensions;
    SourceSpan source;
};

struct ObservationDecodeResult {
    std::vector<Observation> observations;
    std::optional<Diagnostic> error;

    [[nodiscard]] bool ok() const noexcept { return !error.has_value(); }
};

[[nodiscard]] ObservationDecodeResult
decode_observation_json_lines(std::string_view input, StringId file,
                              const ModelLimits &limits = kDefaultModelLimits);

} // namespace ltfa
