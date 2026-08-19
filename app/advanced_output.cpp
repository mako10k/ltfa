#include "advanced_output.hpp"

#include <iomanip>
#include <sstream>
#include <string_view>

namespace ltfa::app {
namespace {

void json_string(std::ostream &output, std::string_view value) {
    output << std::quoted(std::string{value});
}

[[nodiscard]] std::string_view analysis_status(AnalysisStatus status) noexcept {
    switch (status) {
    case AnalysisStatus::complete:
        return "complete";
    case AnalysisStatus::incomplete:
        return "incomplete";
    case AnalysisStatus::invalid:
        return "invalid";
    }
    return "invalid";
}

[[nodiscard]] std::string_view recommendation_status(RecommendationStatus status) noexcept {
    switch (status) {
    case RecommendationStatus::complete:
        return "complete";
    case RecommendationStatus::incomplete:
        return "incomplete";
    case RecommendationStatus::invalid:
        return "invalid";
    }
    return "invalid";
}

[[nodiscard]] std::string_view prediction_name(PredictedProbeOutcome outcome) noexcept {
    switch (outcome) {
    case PredictedProbeOutcome::pass:
        return "pass";
    case PredictedProbeOutcome::non_pass:
        return "non-pass";
    case PredictedProbeOutcome::unknown:
        return "unknown";
    }
    return "unknown";
}

[[nodiscard]] std::string_view cut_id(const NormalizedModel &model,
                                      const CutElement &element) noexcept {
    switch (element.kind) {
    case CutElementKind::object:
        return model.external_id(element.object_value);
    case CutElementKind::relation:
        return model.string(model.relations[element.relation_value.value()].external_id);
    case CutElementKind::failure_domain:
        return model.string(
            model.failure_domains[element.failure_domain_value.value()].external_id);
    }
    return {};
}

[[nodiscard]] std::string_view cut_kind(CutElementKind kind) noexcept {
    switch (kind) {
    case CutElementKind::object:
        return "object";
    case CutElementKind::relation:
        return "relation";
    case CutElementKind::failure_domain:
        return "failure-domain";
    }
    return "object";
}

} // namespace

std::string render_service_cuts_human(const NormalizedModel &model,
                                      const ServiceCutAnalysisResult &result) {
    std::ostringstream output;
    output << "LTFA SERVICE CUTS\nstatus: " << analysis_status(result.status)
           << "\nbaseline-reachable: " << (result.baseline_reachable ? "true" : "false")
           << "\nminimum-object-cut: " << result.minimum_object_cut
           << "\nminimum-relation-cut: " << result.minimum_relation_cut << "\nfindings:\n";
    for (const auto &finding : result.findings) {
        output << "  -";
        for (const auto &element : finding.elements) {
            output << ' ' << cut_kind(element.kind) << ':' << cut_id(model, element);
        }
        output << '\n';
    }
    output << "usage: candidate-sets=" << result.usage.candidate_sets_considered
           << " visited-objects=" << result.usage.visited_objects
           << " examined-relations=" << result.usage.examined_relations
           << " flow-transitions=" << result.usage.flow_transitions << '\n';
    return output.str();
}

std::string render_service_cuts_json(const NormalizedModel &model,
                                     const ServiceCutAnalysisResult &result) {
    std::ostringstream output;
    output << "{\"schema\":\"ltfa.service-cuts/v1\",\"status\":";
    json_string(output, analysis_status(result.status));
    output << ",\"baseline_reachable\":" << (result.baseline_reachable ? "true" : "false")
           << ",\"source\":";
    json_string(output, model.external_id(result.request.source));
    output << ",\"target\":";
    json_string(output, model.external_id(result.request.target));
    output << ",\"minimum_object_cut\":" << result.minimum_object_cut
           << ",\"minimum_relation_cut\":" << result.minimum_relation_cut << ",\"findings\":[";
    for (std::size_t index = 0; index < result.findings.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        output << "{\"elements\":[";
        const auto &finding = result.findings[index];
        for (std::size_t element_index = 0; element_index < finding.elements.size();
             ++element_index) {
            if (element_index != 0) {
                output << ',';
            }
            output << "{\"kind\":";
            json_string(output, cut_kind(finding.elements[element_index].kind));
            output << ",\"id\":";
            json_string(output, cut_id(model, finding.elements[element_index]));
            output << '}';
        }
        output << "]}";
    }
    output << "],\"resource_budget_usage\":{\"candidate_sets\":"
           << result.usage.candidate_sets_considered
           << ",\"visited_objects\":" << result.usage.visited_objects
           << ",\"examined_relations\":" << result.usage.examined_relations
           << ",\"flow_transitions\":" << result.usage.flow_transitions << "}}\n";
    return output.str();
}

std::string render_diagnosis_cli_human(const NormalizedModel &model,
                                       const DiagnosisResult &diagnosis,
                                       const ObservationRecommendationResult &recommendations) {
    std::ostringstream output;
    output << "LTFA DIAGNOSIS\nstatus: " << to_string(diagnosis.status) << "\nfindings:\n";
    for (const auto &finding : diagnosis.findings) {
        output << "  - rank=" << finding.rank << " tied=" << (finding.tied ? "true" : "false")
               << " hypothesis=" << finding.hypothesis.stable_id
               << " contradictions=" << finding.contradictions << '\n';
    }
    output << "next-observations: " << recommendation_status(recommendations.status) << '\n';
    for (const auto &recommendation : recommendations.recommendations) {
        output << "  - rank=" << recommendation.diagnosis_rank
               << " probe=" << model.string(model.probes[recommendation.probe.value()].external_id)
               << " separated-pairs=" << recommendation.separated_pairs << '\n';
    }
    return output.str();
}

std::string render_diagnosis_cli_json(const NormalizedModel &model, const EvidenceIndex &evidence,
                                      const DiagnosisResult &diagnosis,
                                      const ObservationRecommendationResult &recommendations,
                                      const DiagnosisOutputContext &context) {
    std::ostringstream output;
    output << "{\"schema\":\"ltfa.diagnosis-cli/v1\",\"diagnosis\":"
           << render_diagnosis_json(model, evidence, diagnosis, context)
           << ",\"next_observations\":{\"status\":";
    json_string(output, recommendation_status(recommendations.status));
    output << ",\"recommendations\":[";
    for (std::size_t index = 0; index < recommendations.recommendations.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        const auto &recommendation = recommendations.recommendations[index];
        output << "{\"diagnosis_rank\":" << recommendation.diagnosis_rank << ",\"probe\":";
        json_string(output, model.string(model.probes[recommendation.probe.value()].external_id));
        output << ",\"separated_pairs\":" << recommendation.separated_pairs << ",\"predictions\":[";
        for (std::size_t prediction = 0; prediction < recommendation.predictions.size();
             ++prediction) {
            if (prediction != 0) {
                output << ',';
            }
            output << "{\"hypothesis\":";
            json_string(output, recommendation.predictions[prediction].hypothesis_id);
            output << ",\"outcome\":";
            json_string(output, prediction_name(recommendation.predictions[prediction].outcome));
            output << '}';
        }
        output << "]}";
    }
    output << "],\"resource_budget_usage\":{\"candidate_probes\":"
           << recommendations.usage.candidate_probes
           << ",\"hypothesis_comparisons\":" << recommendations.usage.hypothesis_comparisons
           << "}}}\n";
    return output.str();
}

} // namespace ltfa::app
