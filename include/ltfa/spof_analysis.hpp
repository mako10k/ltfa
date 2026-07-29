#pragma once

#include "ltfa/path_analysis.hpp"

#include <cstddef>
#include <string_view>
#include <vector>

namespace ltfa {

struct StructuralAnalysisResult {
    AnalysisStatus status{AnalysisStatus::invalid};
    GraphFilter view;
    std::vector<ObjectRef> sources;
    std::vector<ObjectRef> targets;
    std::vector<ObjectRef> articulation_points;
    std::vector<RelationId> bridges;
    AssuranceQualification qualification;
    bool contains_unknown_availability{false};
    bool alternatives_excluded_by_filter{false};
    TraversalUsage usage;
};

enum class SpofClassification {
    confirmed_spof,
    model_spof,
    not_a_spof_in_view,
};

[[nodiscard]] std::string_view to_string(SpofClassification classification) noexcept;

struct SpofPathEvidence {
    ObjectRef target;
    std::vector<ObjectRef> objects;
    std::vector<RelationId> relations;
};

struct SpofFinding {
    RemovalTarget candidate;
    SpofClassification classification{SpofClassification::model_spof};
    std::vector<ObjectRef> affected_targets;
    std::vector<ObjectRef> affected_objects;
    std::vector<ObjectRef> common_cause_members;
    std::vector<FailureDomainId> common_domains;
    std::vector<SpofPathEvidence> baseline_paths;
    std::vector<SpofPathEvidence> retained_paths;
    std::vector<ObjectRef> remaining_unknowns;
    AssuranceQualification qualification;
    bool alternatives_excluded_by_filter{false};
};

struct SpofRequest {
    ObjectRef source;
    std::vector<ObjectRef> targets;
    GraphFilter view;
    std::vector<RemovalTarget> candidates;
};

struct SpofBudget {
    std::size_t max_candidates{1'024};
    std::size_t max_total_visited_objects{2'000'000};
    std::size_t max_total_examined_relations{10'000'000};
};

struct SpofUsage {
    std::size_t candidates_considered{0};
    std::size_t visited_objects{0};
    std::size_t examined_relations{0};
};

struct SpofAnalysisResult {
    AnalysisStatus status{AnalysisStatus::invalid};
    SpofRequest request;
    StructuralAnalysisResult structural;
    std::vector<SpofFinding> findings;
    SpofUsage usage;
};

[[nodiscard]] StructuralAnalysisResult analyze_articulation_bridges(
    const NormalizedModel &model, const AvailabilitySnapshot &snapshot,
    const GraphFilter &view = {}, const std::vector<ObjectRef> &sources = {},
    const std::vector<ObjectRef> &targets = {}, const TraversalBudget &budget = {});

[[nodiscard]] SpofAnalysisResult analyze_spof(const NormalizedModel &model,
                                              const AvailabilitySnapshot &snapshot,
                                              const SpofRequest &request,
                                              const SpofBudget &budget = {});

} // namespace ltfa
