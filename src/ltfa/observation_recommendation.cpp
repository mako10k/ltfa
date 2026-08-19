#include "ltfa/observation_recommendation.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>

namespace ltfa {
namespace {

[[nodiscard]] bool valid_object(const NormalizedModel &model, ObjectRef object) noexcept {
    return object.is_valid() &&
           (object.kind == ObjectKind::entity ? object.index < model.entities.size()
                                              : object.index < model.endpoints.size());
}

[[nodiscard]] bool valid_model(const NormalizedModel &model) noexcept {
    for (std::size_t index = 0; index < model.probes.size(); ++index) {
        const auto &probe = model.probes[index];
        if (probe.id != ProbeId{static_cast<std::uint32_t>(index)} ||
            !valid_object(model, probe.target) || !valid_object(model, probe.vantage) ||
            model.string(probe.external_id).empty()) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool valid_evidence(const NormalizedModel &model,
                                  const EvidenceIndex &evidence) noexcept {
    for (const auto &indexed : evidence.observations) {
        if (!indexed.probe.is_valid() || indexed.probe.value() >= model.probes.size()) {
            return false;
        }
    }
    for (const auto &group : evidence.groups) {
        if (!group.probe.is_valid() || group.probe.value() >= model.probes.size()) {
            return false;
        }
        const auto &probe = model.probes[group.probe.value()];
        if (group.target != probe.target || group.vantage != probe.vantage ||
            group.latest.begin > evidence.observations.size() ||
            group.latest.count > evidence.observations.size() - group.latest.begin ||
            group.history.begin > evidence.observations.size() ||
            group.history.count > evidence.observations.size() - group.history.begin) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool valid_diagnosis(const NormalizedModel &model,
                                   const DiagnosisResult &diagnosis) noexcept {
    for (const auto &finding : diagnosis.findings) {
        if (finding.rank == 0 || finding.hypothesis.stable_id.empty() ||
            !valid_object(model, finding.hypothesis.target)) {
            return false;
        }
        for (const auto object : finding.affected_objects) {
            if (!valid_object(model, object)) {
                return false;
            }
        }
        for (const auto object : finding.remaining_unknown_objects) {
            if (!valid_object(model, object)) {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] bool contains(const std::vector<ObjectRef> &objects, ObjectRef value) noexcept {
    return std::find(objects.begin(), objects.end(), value) != objects.end();
}

[[nodiscard]] PredictedProbeOutcome predict(const DiagnosisFinding &finding,
                                            const Probe &probe) noexcept {
    if (contains(finding.remaining_unknown_objects, probe.target) ||
        contains(finding.remaining_unknown_objects, probe.vantage)) {
        return PredictedProbeOutcome::unknown;
    }
    return contains(finding.affected_objects, probe.target) ||
                   contains(finding.affected_objects, probe.vantage)
               ? PredictedProbeOutcome::non_pass
               : PredictedProbeOutcome::pass;
}

[[nodiscard]] bool separates(PredictedProbeOutcome left, PredictedProbeOutcome right) noexcept {
    return left != PredictedProbeOutcome::unknown && right != PredictedProbeOutcome::unknown &&
           left != right;
}

[[nodiscard]] std::vector<ProbeId> absent_probes(const NormalizedModel &model,
                                                 const EvidenceIndex &evidence) {
    std::vector<bool> current(model.probes.size(), false);
    for (const auto &indexed : evidence.observations) {
        if (indexed.freshness == EvidenceFreshness::current) {
            current[indexed.probe.value()] = true;
        }
    }
    std::vector<ProbeId> result;
    for (const auto &probe : model.probes) {
        if (!current[probe.id.value()]) {
            result.push_back(probe.id);
        }
    }
    std::sort(result.begin(), result.end(), [&](ProbeId left, ProbeId right) {
        return model.string(model.probes[left.value()].external_id) <
               model.string(model.probes[right.value()].external_id);
    });
    return result;
}

[[nodiscard]] bool better_recommendation(const NormalizedModel &model,
                                         const ObservationRecommendation &left,
                                         const ObservationRecommendation &right) noexcept {
    if (left.separated_pairs != right.separated_pairs) {
        return left.separated_pairs > right.separated_pairs;
    }
    return model.string(model.probes[left.probe.value()].external_id) <
           model.string(model.probes[right.probe.value()].external_id);
}

} // namespace

ObservationRecommendationResult
recommend_next_observations(const NormalizedModel &model, const EvidenceIndex &evidence,
                            const DiagnosisResult &diagnosis,
                            const ObservationRecommendationBudget &budget) {
    ObservationRecommendationResult result;
    if (!valid_model(model) || !valid_evidence(model, evidence) ||
        !valid_diagnosis(model, diagnosis) || diagnosis.status == DiagnosisStatus::invalid) {
        return result;
    }
    if (diagnosis.status == DiagnosisStatus::incomplete) {
        result.status = RecommendationStatus::incomplete;
        return result;
    }
    const auto probes = absent_probes(model, evidence);
    result.status = RecommendationStatus::complete;
    for (std::size_t begin = 0; begin < diagnosis.findings.size();) {
        const auto rank = diagnosis.findings[begin].rank;
        auto end = begin + 1;
        while (end < diagnosis.findings.size() && diagnosis.findings[end].rank == rank) {
            ++end;
        }
        if (end - begin < 2 ||
            !std::all_of(diagnosis.findings.begin() + static_cast<std::ptrdiff_t>(begin),
                         diagnosis.findings.begin() + static_cast<std::ptrdiff_t>(end),
                         [](const DiagnosisFinding &finding) { return finding.tied; })) {
            begin = end;
            continue;
        }
        std::optional<ObservationRecommendation> best;
        for (const auto probe_id : probes) {
            if (result.usage.candidate_probes >= budget.max_candidate_probes) {
                result.status = RecommendationStatus::incomplete;
                return result;
            }
            ++result.usage.candidate_probes;
            ObservationRecommendation candidate;
            candidate.diagnosis_rank = rank;
            candidate.probe = probe_id;
            const auto &probe = model.probes[probe_id.value()];
            for (std::size_t index = begin; index < end; ++index) {
                candidate.predictions.push_back({diagnosis.findings[index].hypothesis.stable_id,
                                                 predict(diagnosis.findings[index], probe)});
            }
            for (std::size_t left = 0; left < candidate.predictions.size(); ++left) {
                for (std::size_t right = left + 1; right < candidate.predictions.size(); ++right) {
                    if (result.usage.hypothesis_comparisons >= budget.max_hypothesis_comparisons) {
                        result.status = RecommendationStatus::incomplete;
                        return result;
                    }
                    ++result.usage.hypothesis_comparisons;
                    if (separates(candidate.predictions[left].outcome,
                                  candidate.predictions[right].outcome)) {
                        ++candidate.separated_pairs;
                    }
                }
            }
            if (candidate.separated_pairs > 0 &&
                (!best.has_value() || better_recommendation(model, candidate, *best))) {
                best = std::move(candidate);
            }
        }
        if (best.has_value()) {
            if (result.recommendations.size() >= budget.max_recommendations) {
                result.status = RecommendationStatus::incomplete;
                return result;
            }
            result.recommendations.push_back(std::move(*best));
        }
        begin = end;
    }
    return result;
}

} // namespace ltfa
