#pragma once

#include "ltfa/evidence.hpp"
#include "ltfa/failure_propagation.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ltfa {

enum class DiagnosisStatus {
    complete,
    incomplete,
    invalid,
};

enum class DiagnosisHypothesisKind {
    declared_failure_mode,
    object_failure,
};

enum class DiagnosisEvidenceAssessment {
    supports_hypothesis,
    contradicts_hypothesis,
    consistent,
    unexplained_non_pass,
    unknown,
};

enum class DiagnosisIncompleteReason {
    causal_cone_budget,
    hypothesis_budget,
    simulation_budget,
    evidence_comparison_budget,
    propagation_budget,
};

[[nodiscard]] std::string_view to_string(DiagnosisStatus value) noexcept;
[[nodiscard]] std::string_view to_string(DiagnosisHypothesisKind value) noexcept;
[[nodiscard]] std::string_view to_string(DiagnosisEvidenceAssessment value) noexcept;
[[nodiscard]] std::string_view to_string(DiagnosisIncompleteReason value) noexcept;

struct DiagnosisHypothesis {
    DiagnosisHypothesisKind kind{DiagnosisHypothesisKind::object_failure};
    ObjectRef target;
    FailureModeId failure_mode;
    std::string stable_id;
};

struct DiagnosisEvidence {
    std::uint32_t observation_index{0};
    DiagnosisEvidenceAssessment assessment{DiagnosisEvidenceAssessment::unknown};
};

struct DiagnosisFinding {
    std::uint32_t rank{0};
    bool tied{false};
    DiagnosisHypothesis hypothesis;
    std::size_t contradictions{0};
    std::size_t explained_non_pass{0};
    std::size_t unexplained_non_pass{0};
    std::size_t current_evidence{0};
    std::size_t stale_evidence{0};
    std::vector<ObjectRef> affected_objects;
    std::vector<DiagnosisEvidence> evidence;
    std::vector<std::uint32_t> conflicting_evidence;
    std::vector<ObjectRef> remaining_unknown_objects;
    std::vector<std::uint32_t> remaining_unknown_evidence;
    AssuranceQualification qualification;
};

struct DiagnosisBudget {
    std::size_t max_causal_objects{100'000};
    std::size_t max_hypotheses{10'000};
    std::size_t max_simulations{10'000};
    std::size_t max_evidence_comparisons{1'000'000};
    PropagationBudget propagation;
};

struct DiagnosisUsage {
    std::size_t causal_objects{0};
    std::size_t candidate_hypotheses{0};
    std::size_t generated_hypotheses{0};
    std::size_t simulations{0};
    std::size_t evidence_comparisons{0};
};

struct DiagnosisResult {
    DiagnosisStatus status{DiagnosisStatus::invalid};
    std::vector<ObjectRef> causal_cone;
    std::vector<DiagnosisFinding> findings;
    std::vector<DiagnosisIncompleteReason> incomplete_reasons;
    DiagnosisUsage usage;
};

[[nodiscard]] DiagnosisResult diagnose_observations(const NormalizedModel &model,
                                                    const EvidenceIndex &evidence,
                                                    const DiagnosisBudget &budget = {});

} // namespace ltfa
