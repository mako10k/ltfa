#pragma once

#include "ltfa/cut_analysis.hpp"
#include "ltfa/diagnosis.hpp"
#include "ltfa/diagnosis_output.hpp"
#include "ltfa/observation_recommendation.hpp"

#include <string>

namespace ltfa::app {

[[nodiscard]] std::string render_service_cuts_human(const NormalizedModel &model,
                                                    const ServiceCutAnalysisResult &result);
[[nodiscard]] std::string render_service_cuts_json(const NormalizedModel &model,
                                                   const ServiceCutAnalysisResult &result);
[[nodiscard]] std::string
render_diagnosis_cli_human(const NormalizedModel &model, const DiagnosisResult &diagnosis,
                           const ObservationRecommendationResult &recommendations);
[[nodiscard]] std::string
render_diagnosis_cli_json(const NormalizedModel &model, const EvidenceIndex &evidence,
                          const DiagnosisResult &diagnosis,
                          const ObservationRecommendationResult &recommendations,
                          const DiagnosisOutputContext &context = {});

} // namespace ltfa::app
