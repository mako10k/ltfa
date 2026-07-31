#pragma once

#include "ltfa/error.hpp"
#include "ltfa/model.hpp"
#include "ltfa/observation.hpp"

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace ltfa {

enum class EvidenceFreshness {
    current,
    stale,
};

enum class EvidenceMeaning {
    registered_probe_outcome,
    vantage_reachability,
};

enum class EvidenceConflictKind {
    same_probe_and_vantage,
    across_vantages,
};

[[nodiscard]] std::string_view to_string(EvidenceFreshness value) noexcept;
[[nodiscard]] std::string_view to_string(EvidenceMeaning value) noexcept;
[[nodiscard]] std::string_view to_string(EvidenceConflictKind value) noexcept;

struct IndexedObservation {
    Observation observation;
    ProbeId probe;
    EvidenceFreshness freshness{EvidenceFreshness::stale};
    EvidenceMeaning meaning{EvidenceMeaning::registered_probe_outcome};
    bool conflicting{false};
};

struct EvidenceGroup {
    ProbeId probe;
    ObjectRef target;
    ObjectRef vantage;
    ObservationKind kind{ObservationKind::icmp};
    IndexRange latest;
    IndexRange history;
    bool latest_conflicts{false};
};

struct EvidenceConflict {
    EvidenceConflictKind kind{EvidenceConflictKind::same_probe_and_vantage};
    ObjectRef target;
    ObservationKind observation_kind{ObservationKind::icmp};
    std::vector<std::uint32_t> observation_indices;
};

struct EvidenceIndex {
    ObservationTimestamp analysis_at;
    std::vector<IndexedObservation> observations;
    std::vector<EvidenceGroup> groups;
    std::vector<EvidenceConflict> conflicts;
};

struct EvidenceIndexResult {
    std::optional<EvidenceIndex> index;
    std::optional<Diagnostic> error;

    [[nodiscard]] bool ok() const noexcept { return index.has_value() && !error.has_value(); }
};

[[nodiscard]] EvidenceIndexResult build_evidence_index(const NormalizedModel &model,
                                                       std::vector<Observation> observations,
                                                       ObservationTimestamp analysis_at);

} // namespace ltfa
