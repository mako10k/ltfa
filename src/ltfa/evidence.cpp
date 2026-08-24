#include "ltfa/evidence.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace ltfa {
namespace {

struct Instant {
    std::int64_t seconds{0};
    std::uint32_t nanosecond{0};
};

struct ResolvedObservation {
    Observation observation;
    ProbeId probe;
    Instant observed_at;
};

struct ProbeLookup {
    std::string_view external_id;
    std::uint32_t index{0};
};

struct TargetKind {
    ObjectKind object_kind{ObjectKind::entity};
    std::uint32_t object_index{0};
    ObservationKind observation_kind{ObservationKind::icmp};
};

using EvidenceSignal = std::tuple<ObservationOutcome, std::optional<ObservationNumber>,
                                  std::string_view, std::string_view>;

[[nodiscard]] bool operator<(const TargetKind &left, const TargetKind &right) noexcept {
    return std::tie(left.object_kind, left.object_index, left.observation_kind) <
           std::tie(right.object_kind, right.object_index, right.observation_kind);
}

[[nodiscard]] bool instant_less(Instant left, Instant right) noexcept {
    return std::tie(left.seconds, left.nanosecond) < std::tie(right.seconds, right.nanosecond);
}

[[nodiscard]] bool instant_equal(Instant left, Instant right) noexcept {
    return left.seconds == right.seconds && left.nanosecond == right.nanosecond;
}

[[nodiscard]] bool is_leap_year(std::uint16_t year) noexcept {
    return (year % 4U == 0U && year % 100U != 0U) || year % 400U == 0U;
}

[[nodiscard]] std::uint8_t days_in_month(std::uint16_t year, std::uint8_t month) noexcept {
    constexpr std::uint8_t days[]{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 0 || month > 12) {
        return 0;
    }
    if (month == 2 && is_leap_year(year)) {
        return 29;
    }
    return days[month - 1];
}

[[nodiscard]] std::int64_t days_from_civil(std::int64_t year, std::uint8_t month,
                                           std::uint8_t day) noexcept {
    year -= month <= 2 ? 1 : 0;
    const auto era = (year >= 0 ? year : year - 399) / 400;
    const auto year_of_era = static_cast<std::uint32_t>(year - era * 400);
    const auto adjusted_month = static_cast<std::uint32_t>(month) + (month > 2 ? -3 : 9);
    const auto day_of_year =
        (153U * adjusted_month + 2U) / 5U + static_cast<std::uint32_t>(day) - 1U;
    const auto day_of_era =
        year_of_era * 365U + year_of_era / 4U - year_of_era / 100U + day_of_year;
    return era * 146097 + static_cast<std::int64_t>(day_of_era) - 719468;
}

[[nodiscard]] std::optional<Instant> to_instant(const ObservationTimestamp &timestamp) noexcept {
    if (timestamp.year == 0 || timestamp.month == 0 || timestamp.month > 12 || timestamp.day == 0 ||
        timestamp.day > days_in_month(timestamp.year, timestamp.month) || timestamp.hour > 23 ||
        timestamp.minute > 59 || timestamp.second > 60 || timestamp.nanosecond >= 1'000'000'000U ||
        timestamp.fractional_digits > 9 ||
        (timestamp.fractional_digits == 0 && timestamp.nanosecond != 0) ||
        timestamp.offset_minutes < -1439 || timestamp.offset_minutes > 1439) {
        return std::nullopt;
    }

    const auto seconds = days_from_civil(timestamp.year, timestamp.month, timestamp.day) * 86400 +
                         static_cast<std::int64_t>(timestamp.hour) * 3600 +
                         static_cast<std::int64_t>(timestamp.minute) * 60 + timestamp.second -
                         static_cast<std::int64_t>(timestamp.offset_minutes) * 60;
    return Instant{seconds, timestamp.nanosecond};
}

[[nodiscard]] EvidenceFreshness freshness(Instant observed_at, Instant analysis_at,
                                          std::uint64_t valid_for_ms) noexcept {
    auto elapsed_seconds = analysis_at.seconds - observed_at.seconds;
    auto elapsed_nanoseconds = static_cast<std::int64_t>(analysis_at.nanosecond) -
                               static_cast<std::int64_t>(observed_at.nanosecond);
    if (elapsed_nanoseconds < 0) {
        --elapsed_seconds;
        elapsed_nanoseconds += 1'000'000'000;
    }

    const auto elapsed_seconds_unsigned = static_cast<std::uint64_t>(elapsed_seconds);
    const auto valid_seconds = valid_for_ms / 1000U;
    const auto valid_nanoseconds = static_cast<std::uint32_t>(valid_for_ms % 1000U) * 1'000'000U;
    if (elapsed_seconds_unsigned < valid_seconds) {
        return EvidenceFreshness::current;
    }
    if (elapsed_seconds_unsigned > valid_seconds) {
        return EvidenceFreshness::stale;
    }
    return elapsed_nanoseconds <= valid_nanoseconds ? EvidenceFreshness::current
                                                    : EvidenceFreshness::stale;
}

[[nodiscard]] Diagnostic make_error(DiagnosticCode code, SourceSpan source, std::string message) {
    return {
        code, DiagnosticSeverity::error, source, std::nullopt, std::nullopt, std::move(message),
    };
}

[[nodiscard]] std::optional<ObservationKind> observation_kind(ProbeKind kind) noexcept {
    switch (kind) {
    case ProbeKind::icmp:
        return ObservationKind::icmp;
    case ProbeKind::tcp_connect:
        return ObservationKind::tcp_connect;
    case ProbeKind::custom_heartbeat:
        return ObservationKind::custom_heartbeat;
    case ProbeKind::metric:
        return ObservationKind::metric;
    case ProbeKind::discrete_state:
        return ObservationKind::discrete_state;
    }
    return std::nullopt;
}

[[nodiscard]] bool has_custom_heartbeat_mapping(const NormalizedModel &model,
                                                const Probe &probe) noexcept {
    if (probe.kind != ProbeKind::custom_heartbeat) {
        return true;
    }
    const auto begin = static_cast<std::size_t>(probe.attributes.begin);
    const auto end = begin + probe.attributes.count;
    if (begin > model.attributes.size() || end > model.attributes.size()) {
        return false;
    }
    return std::any_of(model.attributes.begin() + static_cast<std::ptrdiff_t>(begin),
                       model.attributes.begin() + static_cast<std::ptrdiff_t>(end),
                       [&model](const ModelAttribute &attribute) {
                           return attribute.extension &&
                                  model.string(attribute.key) == "x-outcome-map" &&
                                  !model.string(attribute.value).empty();
                       });
}

[[nodiscard]] auto optional_text(const std::optional<std::string> &value) noexcept {
    return value.has_value() ? std::string_view{*value} : std::string_view{};
}

[[nodiscard]] EvidenceSignal evidence_signal(const Observation &observation) noexcept {
    return {observation.outcome, observation.value, optional_text(observation.unit),
            optional_text(observation.state)};
}

[[nodiscard]] bool resolved_less(const ResolvedObservation &left,
                                 const ResolvedObservation &right) noexcept {
    if (left.observation.probe_id != right.observation.probe_id) {
        return left.observation.probe_id < right.observation.probe_id;
    }
    if (left.observation.vantage != right.observation.vantage) {
        return left.observation.vantage < right.observation.vantage;
    }
    if (!instant_equal(left.observed_at, right.observed_at)) {
        return instant_less(right.observed_at, left.observed_at);
    }
    return std::make_tuple(
               left.observation.outcome, optional_text(left.observation.producer_id),
               left.observation.producer_sequence, optional_text(left.observation.evidence_ref),
               optional_text(left.observation.detail), left.observation.source.file.value(),
               left.observation.source.begin.byte_offset) <
           std::make_tuple(
               right.observation.outcome, optional_text(right.observation.producer_id),
               right.observation.producer_sequence, optional_text(right.observation.evidence_ref),
               optional_text(right.observation.detail), right.observation.source.file.value(),
               right.observation.source.begin.byte_offset);
}

[[nodiscard]] bool same_group(const ResolvedObservation &left,
                              const ResolvedObservation &right) noexcept {
    return left.probe == right.probe;
}

void mark_conflict(EvidenceIndex &index, EvidenceConflict conflict) {
    for (const auto observation_index : conflict.observation_indices) {
        index.observations[observation_index].conflicting = true;
    }
    index.conflicts.push_back(std::move(conflict));
}

} // namespace

std::string_view to_string(EvidenceFreshness value) noexcept {
    switch (value) {
    case EvidenceFreshness::current:
        return "current";
    case EvidenceFreshness::stale:
        return "stale";
    }
    return "unknown";
}

std::string_view to_string(EvidenceMeaning value) noexcept {
    switch (value) {
    case EvidenceMeaning::registered_probe_outcome:
        return "registered-probe-outcome";
    case EvidenceMeaning::vantage_reachability:
        return "vantage-reachability";
    }
    return "unknown";
}

std::string_view to_string(EvidenceConflictKind value) noexcept {
    switch (value) {
    case EvidenceConflictKind::same_probe_and_vantage:
        return "same-probe-and-vantage";
    case EvidenceConflictKind::across_vantages:
        return "across-vantages";
    }
    return "unknown";
}

EvidenceIndexResult build_evidence_index(const NormalizedModel &model,
                                         std::vector<Observation> observations,
                                         ObservationTimestamp analysis_at) {
    if (observations.size() > std::numeric_limits<std::uint32_t>::max()) {
        return {std::nullopt, make_error(DiagnosticCode::limit_observation_records, {},
                                         "evidence index exceeds the 32-bit observation range")};
    }
    const auto analysis_instant = to_instant(analysis_at);
    if (!analysis_instant.has_value()) {
        return {std::nullopt,
                make_error(DiagnosticCode::observation_invalid_reference_time, {},
                           "analysis_at must be a valid RFC 3339 instant with a known offset")};
    }

    std::vector<ProbeLookup> probes;
    probes.reserve(model.probes.size());
    for (std::uint32_t index = 0; index < model.probes.size(); ++index) {
        probes.push_back({model.string(model.probes[index].external_id), index});
    }
    std::sort(probes.begin(), probes.end(), [](const ProbeLookup &left, const ProbeLookup &right) {
        return std::tie(left.external_id, left.index) < std::tie(right.external_id, right.index);
    });

    std::vector<ResolvedObservation> resolved;
    resolved.reserve(observations.size());
    for (auto &observation : observations) {
        const auto found = std::lower_bound(
            probes.begin(), probes.end(), observation.probe_id,
            [](const ProbeLookup &probe, std::string_view id) { return probe.external_id < id; });
        if (found == probes.end() || found->external_id != observation.probe_id) {
            return {std::nullopt,
                    make_error(DiagnosticCode::observation_probe_unresolved, observation.source,
                               "observation probe '" + observation.probe_id +
                                   "' is not registered in the model")};
        }
        if (std::next(found) != probes.end() &&
            std::next(found)->external_id == found->external_id) {
            return {std::nullopt,
                    make_error(DiagnosticCode::observation_contract_mismatch, observation.source,
                               "observation probe '" + observation.probe_id +
                                   "' is ambiguous in the normalized model")};
        }

        const auto &probe = model.probes[found->index];
        const auto expected_kind = observation_kind(probe.kind);
        const auto target = model.external_id(probe.target);
        const auto vantage = model.external_id(probe.vantage);
        if (probe.id.value() != found->index || target.empty() || vantage.empty() ||
            !expected_kind.has_value()) {
            return {std::nullopt,
                    make_error(DiagnosticCode::observation_contract_mismatch, observation.source,
                               "registered probe '" + observation.probe_id +
                                   "' has an invalid normalized contract")};
        }
        if (!has_custom_heartbeat_mapping(model, probe)) {
            return {std::nullopt,
                    make_error(DiagnosticCode::observation_contract_mismatch, observation.source,
                               "registered custom-heartbeat probe '" + observation.probe_id +
                                   "' must declare x-outcome-map semantics")};
        }

        std::string mismatch;
        if (observation.target != target) {
            mismatch = "target '" + observation.target + "' does not match registered target '" +
                       std::string{target} + "'";
        } else if (observation.vantage != vantage) {
            mismatch = "vantage '" + observation.vantage + "' does not match registered vantage '" +
                       std::string{vantage} + "'";
        } else if (observation.kind != *expected_kind) {
            mismatch = "kind '" + std::string{to_string(observation.kind)} +
                       "' does not match registered kind '" +
                       std::string{to_string(*expected_kind)} + "'";
        } else if (observation.valid_for_ms != probe.valid_for_ms) {
            mismatch = "valid_for_ms " + std::to_string(observation.valid_for_ms) +
                       " does not match registered validity " + std::to_string(probe.valid_for_ms);
        }
        if (!mismatch.empty()) {
            return {std::nullopt,
                    make_error(DiagnosticCode::observation_contract_mismatch, observation.source,
                               "observation for probe '" + observation.probe_id + "' " + mismatch)};
        }

        const auto observed_instant = to_instant(observation.observed_at);
        if (!observed_instant.has_value()) {
            return {std::nullopt,
                    make_error(DiagnosticCode::observation_invalid_field, observation.source,
                               "observation field 'observed_at' is not a valid instant")};
        }
        if (instant_less(*analysis_instant, *observed_instant)) {
            return {std::nullopt,
                    make_error(DiagnosticCode::observation_after_reference_time, observation.source,
                               "observation time '" + observation.observed_at.text +
                                   "' is after analysis_at '" + analysis_at.text + "'")};
        }
        resolved.push_back({std::move(observation), probe.id, *observed_instant});
    }

    // The observations now live in resolved. Release the moved-from backing array before
    // allocating the indexed representation.
    std::vector<Observation>{}.swap(observations);

    std::sort(resolved.begin(), resolved.end(), resolved_less);

    EvidenceIndex index;
    index.analysis_at = std::move(analysis_at);
    index.observations.reserve(resolved.size());
    for (auto &item : resolved) {
        const auto probe = model.probes[item.probe.value()];
        index.observations.push_back(
            {std::move(item.observation), item.probe,
             freshness(item.observed_at, *analysis_instant, probe.valid_for_ms),
             probe.kind == ProbeKind::icmp ? EvidenceMeaning::vantage_reachability
                                           : EvidenceMeaning::registered_probe_outcome,
             false});
    }

    std::size_t group_begin = 0;
    while (group_begin < resolved.size()) {
        auto group_end = group_begin + 1;
        while (group_end < resolved.size() &&
               same_group(resolved[group_begin], resolved[group_end])) {
            ++group_end;
        }
        auto latest_end = group_begin + 1;
        while (latest_end < group_end &&
               instant_equal(resolved[group_begin].observed_at, resolved[latest_end].observed_at)) {
            ++latest_end;
        }

        const auto probe_id = resolved[group_begin].probe;
        const auto &probe = model.probes[probe_id.value()];
        std::set<EvidenceSignal> signals;
        for (auto item = group_begin; item < latest_end; ++item) {
            signals.insert(evidence_signal(index.observations[item].observation));
        }
        const auto latest_conflicts = signals.size() > 1;
        index.groups.push_back({probe_id,
                                probe.target,
                                probe.vantage,
                                index.observations[group_begin].observation.kind,
                                {static_cast<std::uint32_t>(group_begin),
                                 static_cast<std::uint32_t>(latest_end - group_begin)},
                                {static_cast<std::uint32_t>(latest_end),
                                 static_cast<std::uint32_t>(group_end - latest_end)},
                                latest_conflicts});

        if (latest_conflicts) {
            EvidenceConflict conflict;
            conflict.kind = EvidenceConflictKind::same_probe_and_vantage;
            conflict.target = probe.target;
            conflict.observation_kind = index.observations[group_begin].observation.kind;
            for (auto item = group_begin; item < latest_end; ++item) {
                conflict.observation_indices.push_back(static_cast<std::uint32_t>(item));
            }
            mark_conflict(index, std::move(conflict));
        }
        group_begin = group_end;
    }

    std::map<TargetKind, std::vector<std::uint32_t>> current_by_target;
    for (const auto &group : index.groups) {
        for (std::uint32_t offset = 0; offset < group.latest.count; ++offset) {
            const auto observation_index = group.latest.begin + offset;
            if (index.observations[observation_index].freshness == EvidenceFreshness::current) {
                current_by_target[{group.target.kind, group.target.index, group.kind}].push_back(
                    observation_index);
            }
        }
    }

    for (auto &[key, observation_indices] : current_by_target) {
        std::set<std::string_view> vantages;
        std::set<EvidenceSignal> signals;
        for (const auto observation_index : observation_indices) {
            const auto &observation = index.observations[observation_index].observation;
            vantages.insert(observation.vantage);
            signals.insert(evidence_signal(observation));
        }
        if (vantages.size() < 2 || signals.size() < 2) {
            continue;
        }
        mark_conflict(index, {EvidenceConflictKind::across_vantages,
                              {key.object_kind, key.object_index},
                              key.observation_kind,
                              std::move(observation_indices)});
    }

    return {std::move(index), std::nullopt};
}

} // namespace ltfa
