#pragma once

#include "ltfa/diagnosis.hpp"
#include "ltfa/model_output.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace ltfa {

struct DiagnosisOutputContext {
    std::vector<std::string> model_inputs;
    std::vector<std::string> observation_inputs;
    RestrictedOutputPolicy restricted_policy{RestrictedOutputPolicy::omit};
};

struct DiagnosisDotOutputOptions {
    std::size_t finding_index{0};
};

[[nodiscard]] std::string render_diagnosis_json(const NormalizedModel &model,
                                                const EvidenceIndex &evidence,
                                                const DiagnosisResult &diagnosis,
                                                const DiagnosisOutputContext &context = {});

[[nodiscard]] std::string render_diagnosis_dot(const NormalizedModel &model,
                                               const EvidenceIndex &evidence,
                                               const DiagnosisResult &diagnosis,
                                               const DiagnosisOutputContext &context = {},
                                               const DiagnosisDotOutputOptions &options = {});

} // namespace ltfa
