#pragma once

#include "ltfa/diagnosis.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace ltfa {

enum class RecommendationStatus {
    complete,
    incomplete,
    invalid,
};

enum class PredictedProbeOutcome {
    pass,
    non_pass,
    unknown,
};

struct HypothesisProbePrediction {
    std::string hypothesis_id;
    PredictedProbeOutcome outcome{PredictedProbeOutcome::unknown};
};

struct ObservationRecommendation {
    std::uint32_t diagnosis_rank{0};
    ProbeId probe;
    std::size_t separated_pairs{0};
    std::vector<HypothesisProbePrediction> predictions;
};

struct ObservationRecommendationBudget {
    std::size_t max_candidate_probes{10'000};
    std::size_t max_hypothesis_comparisons{1'000'000};
    std::size_t max_recommendations{1'024};
};

struct ObservationRecommendationUsage {
    std::size_t candidate_probes{0};
    std::size_t hypothesis_comparisons{0};
};

struct ObservationRecommendationResult {
    RecommendationStatus status{RecommendationStatus::invalid};
    std::vector<ObservationRecommendation> recommendations;
    ObservationRecommendationUsage usage;
};

[[nodiscard]] ObservationRecommendationResult
recommend_next_observations(const NormalizedModel &model, const EvidenceIndex &evidence,
                            const DiagnosisResult &diagnosis,
                            const ObservationRecommendationBudget &budget = {});

} // namespace ltfa
