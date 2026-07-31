#include "ltfa/diagnosis.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <queue>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace ltfa {
namespace {

struct Candidate {
    DiagnosisHypothesis hypothesis;
    FailureInjection injection;
};

struct CandidateStableLess {
    [[nodiscard]] bool operator()(const Candidate &left, const Candidate &right) const noexcept {
        return left.hypothesis.stable_id < right.hypothesis.stable_id;
    }
};

[[nodiscard]] std::size_t object_count(const NormalizedModel &model) noexcept {
    return model.entities.size() + model.endpoints.size();
}

[[nodiscard]] bool valid_object(const NormalizedModel &model, ObjectRef object) noexcept {
    if (!object.is_valid()) {
        return false;
    }
    return object.kind == ObjectKind::entity ? object.index < model.entities.size()
                                             : object.index < model.endpoints.size();
}

[[nodiscard]] std::size_t object_ordinal(const NormalizedModel &model, ObjectRef object) noexcept {
    return object.kind == ObjectKind::entity ? object.index : model.entities.size() + object.index;
}

[[nodiscard]] bool valid_range(IndexRange range, std::size_t size) noexcept {
    return range.begin <= size && range.count <= size - range.begin;
}

[[nodiscard]] const ObjectAvailabilityAdjacency *
availability_adjacency(const NormalizedModel &model, ObjectRef object) noexcept {
    if (!valid_object(model, object)) {
        return nullptr;
    }
    if (object.kind == ObjectKind::entity) {
        if (object.index >= model.entity_availability_adjacency.size()) {
            return nullptr;
        }
        return &model.entity_availability_adjacency[object.index];
    }
    if (object.index >= model.endpoint_availability_adjacency.size()) {
        return nullptr;
    }
    return &model.endpoint_availability_adjacency[object.index];
}

[[nodiscard]] bool object_less(const NormalizedModel &model, ObjectRef left,
                               ObjectRef right) noexcept {
    const auto left_id = model.external_id(left);
    const auto right_id = model.external_id(right);
    if (left_id != right_id) {
        return left_id < right_id;
    }
    return std::tie(left.kind, left.index) < std::tie(right.kind, right.index);
}

void sort_objects(const NormalizedModel &model, std::vector<ObjectRef> &objects) {
    std::sort(objects.begin(), objects.end(),
              [&](ObjectRef left, ObjectRef right) { return object_less(model, left, right); });
    objects.erase(std::unique(objects.begin(), objects.end()), objects.end());
}

void add_incomplete_reason(DiagnosisResult &result, DiagnosisIncompleteReason reason) {
    if (std::find(result.incomplete_reasons.begin(), result.incomplete_reasons.end(), reason) ==
        result.incomplete_reasons.end()) {
        result.incomplete_reasons.push_back(reason);
    }
    result.status = DiagnosisStatus::incomplete;
}

[[nodiscard]] bool valid_model_contract(const NormalizedModel &model) noexcept {
    if (model.entity_availability_adjacency.size() != model.entities.size() ||
        model.endpoint_availability_adjacency.size() != model.endpoints.size()) {
        return false;
    }
    for (std::size_t index = 0; index < model.probes.size(); ++index) {
        const auto &probe = model.probes[index];
        if (probe.id != ProbeId{static_cast<std::uint32_t>(index)} ||
            !valid_object(model, probe.target) || !valid_object(model, probe.vantage) ||
            model.string(probe.external_id).empty()) {
            return false;
        }
    }
    for (std::size_t index = 0; index < model.failure_modes.size(); ++index) {
        const auto &mode = model.failure_modes[index];
        if (mode.id != FailureModeId{static_cast<std::uint32_t>(index)} ||
            !valid_object(model, mode.target) || model.string(mode.external_id).empty()) {
            return false;
        }
    }
    for (std::size_t index = 0; index < model.availability_dependencies.size(); ++index) {
        const auto &dependency = model.availability_dependencies[index];
        if (dependency.id != AvailabilityDependencyId{static_cast<std::uint32_t>(index)} ||
            !valid_object(model, dependency.provider) ||
            !valid_object(model, dependency.consumer)) {
            return false;
        }
    }
    for (const auto &adjacency : model.entity_availability_adjacency) {
        if (!valid_range(adjacency.forward, model.availability_adjacency_entries.size()) ||
            !valid_range(adjacency.reverse, model.availability_adjacency_entries.size())) {
            return false;
        }
    }
    for (const auto &adjacency : model.endpoint_availability_adjacency) {
        if (!valid_range(adjacency.forward, model.availability_adjacency_entries.size()) ||
            !valid_range(adjacency.reverse, model.availability_adjacency_entries.size())) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::optional<ObservationKind> observation_kind(ProbeKind kind) noexcept {
    switch (kind) {
    case ProbeKind::icmp:
        return std::optional<ObservationKind>{ObservationKind::icmp};
    case ProbeKind::tcp_connect:
        return std::optional<ObservationKind>{ObservationKind::tcp_connect};
    case ProbeKind::custom_heartbeat:
        return std::optional<ObservationKind>{ObservationKind::custom_heartbeat};
    case ProbeKind::metric:
        return std::optional<ObservationKind>{ObservationKind::metric};
    case ProbeKind::discrete_state:
        return std::optional<ObservationKind>{ObservationKind::discrete_state};
    }
    return std::nullopt;
}

[[nodiscard]] bool valid_evidence_contract(const NormalizedModel &model,
                                           const EvidenceIndex &evidence) noexcept {
    if (evidence.observations.size() > std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }
    std::vector<bool> grouped(evidence.observations.size(), false);
    for (const auto &indexed : evidence.observations) {
        if (!indexed.probe.is_valid() || indexed.probe.value() >= model.probes.size()) {
            return false;
        }
        const auto &probe = model.probes[indexed.probe.value()];
        const auto expected_kind = observation_kind(probe.kind);
        if (indexed.observation.probe_id != model.string(probe.external_id) ||
            indexed.observation.target != model.external_id(probe.target) ||
            indexed.observation.vantage != model.external_id(probe.vantage) ||
            !expected_kind.has_value() || indexed.observation.kind != *expected_kind ||
            indexed.observation.valid_for_ms != probe.valid_for_ms) {
            return false;
        }
    }
    for (const auto &group : evidence.groups) {
        if (!group.probe.is_valid() || group.probe.value() >= model.probes.size() ||
            !valid_range(group.latest, evidence.observations.size()) || group.latest.count == 0 ||
            !valid_range(group.history, evidence.observations.size())) {
            return false;
        }
        const auto &probe = model.probes[group.probe.value()];
        const auto expected_kind = observation_kind(probe.kind);
        if (group.target != probe.target || group.vantage != probe.vantage ||
            !expected_kind.has_value() || group.kind != *expected_kind) {
            return false;
        }
        for (const auto range : {group.latest, group.history}) {
            for (std::uint32_t offset = 0; offset < range.count; ++offset) {
                const auto index = range.begin + offset;
                if (grouped[index] || evidence.observations[index].probe != group.probe) {
                    return false;
                }
                grouped[index] = true;
            }
        }
    }
    if (std::find(grouped.begin(), grouped.end(), false) != grouped.end()) {
        return false;
    }
    for (const auto &conflict : evidence.conflicts) {
        for (const auto index : conflict.observation_indices) {
            if (index >= evidence.observations.size()) {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] bool seeds_causal_cone(const EvidenceIndex &evidence,
                                     const EvidenceGroup &group) noexcept {
    for (std::uint32_t offset = 0; offset < group.latest.count; ++offset) {
        const auto &indexed = evidence.observations[group.latest.begin + offset];
        if (indexed.freshness == EvidenceFreshness::stale) {
            return true;
        }
        switch (indexed.observation.outcome) {
        case ObservationOutcome::fail:
        case ObservationOutcome::timeout:
        case ObservationOutcome::error:
            return true;
        case ObservationOutcome::pass:
        case ObservationOutcome::unknown:
            break;
        }
    }
    return false;
}

[[nodiscard]] bool build_causal_cone(const NormalizedModel &model, const EvidenceIndex &evidence,
                                     const DiagnosisBudget &budget, DiagnosisResult &result) {
    const auto compare_objects = [&](ObjectRef left, ObjectRef right) {
        return object_less(model, left, right);
    };
    std::set<ObjectRef, decltype(compare_objects)> pending(compare_objects);
    std::vector<bool> queued(object_count(model), false);
    const auto enqueue = [&](ObjectRef object) {
        if (!valid_object(model, object)) {
            return false;
        }
        const auto ordinal = object_ordinal(model, object);
        if (!queued[ordinal]) {
            queued[ordinal] = true;
            pending.insert(object);
        }
        return true;
    };

    for (const auto &group : evidence.groups) {
        if (seeds_causal_cone(evidence, group) &&
            (!enqueue(group.target) || !enqueue(group.vantage))) {
            return false;
        }
    }

    while (!pending.empty() && result.causal_cone.size() < budget.max_causal_objects) {
        const auto object = *pending.begin();
        pending.erase(pending.begin());
        result.causal_cone.push_back(object);

        const auto *adjacency = availability_adjacency(model, object);
        if (adjacency == nullptr ||
            !valid_range(adjacency->reverse, model.availability_adjacency_entries.size())) {
            return false;
        }
        for (std::uint32_t offset = 0; offset < adjacency->reverse.count; ++offset) {
            const auto &entry =
                model.availability_adjacency_entries[adjacency->reverse.begin + offset];
            if (!entry.dependency.is_valid() ||
                entry.dependency.value() >= model.availability_dependencies.size()) {
                return false;
            }
            const auto &dependency = model.availability_dependencies[entry.dependency.value()];
            if (dependency.consumer != object || dependency.provider != entry.object ||
                !enqueue(entry.object)) {
                return false;
            }
        }
    }
    if (!pending.empty()) {
        add_incomplete_reason(result, DiagnosisIncompleteReason::causal_cone_budget);
    }
    sort_objects(model, result.causal_cone);
    result.usage.causal_objects = result.causal_cone.size();
    return true;
}

[[nodiscard]] std::vector<Candidate> build_candidates(const NormalizedModel &model,
                                                      const DiagnosisResult &result,
                                                      std::size_t maximum,
                                                      std::size_t &candidate_count) {
    std::priority_queue<Candidate, std::vector<Candidate>, CandidateStableLess> retained;
    const auto consider = [&](Candidate candidate) {
        ++candidate_count;
        if (maximum == 0) {
            return;
        }
        if (retained.size() < maximum) {
            retained.push(std::move(candidate));
            return;
        }
        if (candidate.hypothesis.stable_id < retained.top().hypothesis.stable_id) {
            retained.pop();
            retained.push(std::move(candidate));
        }
    };

    std::vector<std::vector<const FailureMode *>> modes_by_object(object_count(model));
    for (const auto &mode : model.failure_modes) {
        modes_by_object[object_ordinal(model, mode.target)].push_back(&mode);
    }
    for (const auto object : result.causal_cone) {
        for (const auto *mode : modes_by_object[object_ordinal(model, object)]) {
            DiagnosisHypothesis hypothesis;
            hypothesis.kind = DiagnosisHypothesisKind::declared_failure_mode;
            hypothesis.target = object;
            hypothesis.failure_mode = mode->id;
            hypothesis.stable_id = "failure-mode:" + std::string{model.string(mode->external_id)};
            consider({std::move(hypothesis), FailureInjection::for_declared_mode(mode->id)});
        }
        DiagnosisHypothesis hypothesis;
        hypothesis.kind = DiagnosisHypothesisKind::object_failure;
        hypothesis.target = object;
        hypothesis.stable_id = "object:" + std::string{model.external_id(object)};
        consider({std::move(hypothesis), FailureInjection::for_object(object)});
    }

    std::vector<Candidate> candidates;
    candidates.reserve(retained.size());
    while (!retained.empty()) {
        candidates.push_back(retained.top());
        retained.pop();
    }
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate &left, const Candidate &right) {
                  return left.hypothesis.stable_id < right.hypothesis.stable_id;
              });
    return candidates;
}

[[nodiscard]] bool is_non_pass(ObservationOutcome outcome) noexcept {
    return outcome == ObservationOutcome::fail || outcome == ObservationOutcome::timeout ||
           outcome == ObservationOutcome::error;
}

void add_affected_objects(const NormalizedModel &model, const PropagationResult &propagation,
                          DiagnosisFinding &finding) {
    const auto append = [&](const std::vector<ObjectRef> &objects) {
        finding.affected_objects.insert(finding.affected_objects.end(), objects.begin(),
                                        objects.end());
    };
    append(propagation.immediate_unavailable);
    append(propagation.immediate_degraded);
    append(propagation.delayed_unavailable);
    append(propagation.delayed_degraded);
    sort_objects(model, finding.affected_objects);

    finding.remaining_unknown_objects = propagation.unknown_objects;
    finding.remaining_unknown_objects.insert(finding.remaining_unknown_objects.end(),
                                             propagation.detection_gaps.begin(),
                                             propagation.detection_gaps.end());
    sort_objects(model, finding.remaining_unknown_objects);
}

[[nodiscard]] bool assess_evidence(const NormalizedModel &model, const EvidenceIndex &evidence,
                                   const DiagnosisBudget &budget,
                                   const PropagationResult &propagation, DiagnosisUsage &usage,
                                   DiagnosisFinding &finding) {
    for (const auto &group : evidence.groups) {
        const auto &probe = model.probes[group.probe.value()];
        const auto target_state = propagation.final_snapshot.state(probe.target);
        const auto vantage_state = propagation.final_snapshot.state(probe.vantage);
        const bool predicted_loss = target_state == AvailabilityState::unavailable ||
                                    vantage_state == AvailabilityState::unavailable;
        const bool prediction_unknown = target_state == AvailabilityState::unknown ||
                                        target_state == AvailabilityState::degraded ||
                                        vantage_state == AvailabilityState::unknown ||
                                        vantage_state == AvailabilityState::degraded;

        for (std::uint32_t offset = 0; offset < group.latest.count; ++offset) {
            if (usage.evidence_comparisons >= budget.max_evidence_comparisons) {
                return false;
            }
            const auto observation_index = group.latest.begin + offset;
            const auto &indexed = evidence.observations[observation_index];
            ++usage.evidence_comparisons;

            DiagnosisEvidenceAssessment assessment = DiagnosisEvidenceAssessment::unknown;
            if (indexed.freshness == EvidenceFreshness::stale) {
                ++finding.stale_evidence;
            } else {
                ++finding.current_evidence;
                const auto outcome = indexed.observation.outcome;
                if (outcome == ObservationOutcome::unknown || prediction_unknown) {
                    assessment = DiagnosisEvidenceAssessment::unknown;
                } else if (outcome == ObservationOutcome::pass) {
                    if (predicted_loss) {
                        assessment = DiagnosisEvidenceAssessment::contradicts_hypothesis;
                        ++finding.contradictions;
                    } else {
                        assessment = DiagnosisEvidenceAssessment::consistent;
                    }
                } else if (is_non_pass(outcome)) {
                    if (predicted_loss) {
                        assessment = DiagnosisEvidenceAssessment::supports_hypothesis;
                        ++finding.explained_non_pass;
                    } else {
                        assessment = DiagnosisEvidenceAssessment::unexplained_non_pass;
                        ++finding.unexplained_non_pass;
                    }
                }
            }
            finding.evidence.push_back({observation_index, assessment});
            if (indexed.conflicting) {
                finding.conflicting_evidence.push_back(observation_index);
            }
            if (assessment == DiagnosisEvidenceAssessment::unknown ||
                assessment == DiagnosisEvidenceAssessment::unexplained_non_pass) {
                finding.remaining_unknown_evidence.push_back(observation_index);
            }
        }
    }
    return true;
}

[[nodiscard]] int evidence_quality_rank(EvidenceQuality quality) noexcept {
    switch (quality) {
    case EvidenceQuality::confirmed:
        return 0;
    case EvidenceQuality::inferred:
        return 1;
    case EvidenceQuality::unknown:
        return 2;
    }
    return 3;
}

[[nodiscard]] int specificity_rank(DiagnosisHypothesisKind kind) noexcept {
    return kind == DiagnosisHypothesisKind::declared_failure_mode ? 0 : 1;
}

[[nodiscard]] bool finding_better(const DiagnosisFinding &left,
                                  const DiagnosisFinding &right) noexcept {
    if (left.contradictions != right.contradictions) {
        return left.contradictions < right.contradictions;
    }
    if (left.explained_non_pass != right.explained_non_pass) {
        return left.explained_non_pass > right.explained_non_pass;
    }
    if (left.unexplained_non_pass != right.unexplained_non_pass) {
        return left.unexplained_non_pass < right.unexplained_non_pass;
    }
    const auto left_specificity = specificity_rank(left.hypothesis.kind);
    const auto right_specificity = specificity_rank(right.hypothesis.kind);
    if (left_specificity != right_specificity) {
        return left_specificity < right_specificity;
    }
    if (left.current_evidence != right.current_evidence) {
        return left.current_evidence > right.current_evidence;
    }
    if (left.stale_evidence != right.stale_evidence) {
        return left.stale_evidence < right.stale_evidence;
    }
    const auto left_quality = evidence_quality_rank(left.qualification.evidence_quality);
    const auto right_quality = evidence_quality_rank(right.qualification.evidence_quality);
    if (left_quality != right_quality) {
        return left_quality < right_quality;
    }
    return left.hypothesis.stable_id < right.hypothesis.stable_id;
}

[[nodiscard]] bool same_rank(const DiagnosisFinding &left, const DiagnosisFinding &right) noexcept {
    return left.contradictions == right.contradictions &&
           left.explained_non_pass == right.explained_non_pass &&
           left.unexplained_non_pass == right.unexplained_non_pass &&
           specificity_rank(left.hypothesis.kind) == specificity_rank(right.hypothesis.kind) &&
           left.current_evidence == right.current_evidence &&
           left.stale_evidence == right.stale_evidence &&
           evidence_quality_rank(left.qualification.evidence_quality) ==
               evidence_quality_rank(right.qualification.evidence_quality);
}

void assign_ranks(std::vector<DiagnosisFinding> &findings) {
    if (findings.empty()) {
        return;
    }
    std::uint32_t rank = 1;
    std::size_t group_begin = 0;
    while (group_begin < findings.size()) {
        std::size_t group_end = group_begin + 1;
        while (group_end < findings.size() &&
               same_rank(findings[group_begin], findings[group_end])) {
            ++group_end;
        }
        const bool tied = group_end - group_begin > 1;
        for (std::size_t index = group_begin; index < group_end; ++index) {
            findings[index].rank = rank;
            findings[index].tied = tied;
        }
        ++rank;
        group_begin = group_end;
    }
}

} // namespace

std::string_view to_string(DiagnosisStatus value) noexcept {
    switch (value) {
    case DiagnosisStatus::complete:
        return "complete";
    case DiagnosisStatus::incomplete:
        return "incomplete";
    case DiagnosisStatus::invalid:
        return "invalid";
    }
    return "unknown";
}

std::string_view to_string(DiagnosisHypothesisKind value) noexcept {
    switch (value) {
    case DiagnosisHypothesisKind::declared_failure_mode:
        return "declared-failure-mode";
    case DiagnosisHypothesisKind::object_failure:
        return "object-failure";
    }
    return "unknown";
}

std::string_view to_string(DiagnosisEvidenceAssessment value) noexcept {
    switch (value) {
    case DiagnosisEvidenceAssessment::supports_hypothesis:
        return "supports-hypothesis";
    case DiagnosisEvidenceAssessment::contradicts_hypothesis:
        return "contradicts-hypothesis";
    case DiagnosisEvidenceAssessment::consistent:
        return "consistent";
    case DiagnosisEvidenceAssessment::unexplained_non_pass:
        return "unexplained-non-pass";
    case DiagnosisEvidenceAssessment::unknown:
        return "unknown";
    }
    return "unknown";
}

std::string_view to_string(DiagnosisIncompleteReason value) noexcept {
    switch (value) {
    case DiagnosisIncompleteReason::causal_cone_budget:
        return "causal-cone-budget";
    case DiagnosisIncompleteReason::hypothesis_budget:
        return "hypothesis-budget";
    case DiagnosisIncompleteReason::simulation_budget:
        return "simulation-budget";
    case DiagnosisIncompleteReason::evidence_comparison_budget:
        return "evidence-comparison-budget";
    case DiagnosisIncompleteReason::propagation_budget:
        return "propagation-budget";
    }
    return "unknown";
}

DiagnosisResult diagnose_observations(const NormalizedModel &model, const EvidenceIndex &evidence,
                                      const DiagnosisBudget &budget) {
    DiagnosisResult result;
    if (!valid_model_contract(model) || !valid_evidence_contract(model, evidence)) {
        return result;
    }
    result.status = DiagnosisStatus::complete;
    if (!build_causal_cone(model, evidence, budget, result)) {
        result.status = DiagnosisStatus::invalid;
        result.findings.clear();
        return result;
    }

    auto candidates =
        build_candidates(model, result, budget.max_hypotheses, result.usage.candidate_hypotheses);
    if (result.usage.candidate_hypotheses > candidates.size()) {
        add_incomplete_reason(result, DiagnosisIncompleteReason::hypothesis_budget);
    }
    result.usage.generated_hypotheses = candidates.size();
    const auto initial = make_availability_snapshot(model);

    for (const auto &candidate : candidates) {
        if (result.usage.simulations >= budget.max_simulations) {
            add_incomplete_reason(result, DiagnosisIncompleteReason::simulation_budget);
            break;
        }
        ++result.usage.simulations;
        const auto propagation =
            propagate_failure(model, initial, candidate.injection, budget.propagation);
        if (propagation.status == PropagationStatus::invalid) {
            result.status = DiagnosisStatus::invalid;
            result.findings.clear();
            return result;
        }
        if (propagation.status == PropagationStatus::incomplete) {
            add_incomplete_reason(result, DiagnosisIncompleteReason::propagation_budget);
            break;
        }

        DiagnosisFinding finding;
        finding.hypothesis = candidate.hypothesis;
        finding.qualification = propagation.qualification;
        add_affected_objects(model, propagation, finding);
        if (!assess_evidence(model, evidence, budget, propagation, result.usage, finding)) {
            add_incomplete_reason(result, DiagnosisIncompleteReason::evidence_comparison_budget);
            break;
        }
        result.findings.push_back(std::move(finding));
    }

    std::sort(result.findings.begin(), result.findings.end(), finding_better);
    assign_ranks(result.findings);
    std::sort(result.incomplete_reasons.begin(), result.incomplete_reasons.end());
    return result;
}

} // namespace ltfa
