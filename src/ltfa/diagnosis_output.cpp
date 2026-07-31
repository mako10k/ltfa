#include "ltfa/diagnosis_output.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace ltfa {
namespace {

constexpr std::string_view kDiagnosisSchema{"ltfa.diagnosis/v1"};
constexpr std::string_view kDiagnosisDotSchema{"ltfa.diagnosis.dot/v1"};

struct Visibility {
    std::vector<bool> entities;
    std::vector<bool> endpoints;
    std::size_t restricted_objects{0};
};

struct Omissions {
    std::size_t restricted_objects{0};
    std::size_t restricted_observations{0};
    std::size_t restricted_findings{0};
    std::size_t observation_details{0};
};

[[nodiscard]] bool valid_object(const NormalizedModel &model, ObjectRef object) noexcept {
    if (!object.is_valid()) {
        return false;
    }
    return object.kind == ObjectKind::entity ? object.index < model.entities.size()
                                             : object.index < model.endpoints.size();
}

[[nodiscard]] bool object_visible(const Visibility &visibility, ObjectRef object) noexcept {
    if (!object.is_valid()) {
        return false;
    }
    return object.kind == ObjectKind::entity
               ? object.index < visibility.entities.size() && visibility.entities[object.index]
               : object.index < visibility.endpoints.size() && visibility.endpoints[object.index];
}

[[nodiscard]] Visibility build_visibility(const NormalizedModel &model,
                                          RestrictedOutputPolicy policy) {
    const bool include_restricted = policy == RestrictedOutputPolicy::include_explicit;
    Visibility visibility;
    visibility.entities.resize(model.entities.size());
    visibility.endpoints.resize(model.endpoints.size());

    for (const auto &entity : model.entities) {
        visibility.entities[entity.id.value()] =
            include_restricted || entity.sensitivity != Sensitivity::restricted;
    }
    bool changed = true;
    while (!include_restricted && changed) {
        changed = false;
        for (const auto &entity : model.entities) {
            if (visibility.entities[entity.id.value()] && entity.network_space.is_valid() &&
                (entity.network_space.value() >= visibility.entities.size() ||
                 !visibility.entities[entity.network_space.value()])) {
                visibility.entities[entity.id.value()] = false;
                changed = true;
            }
        }
    }
    for (const bool visible : visibility.entities) {
        if (!visible) {
            ++visibility.restricted_objects;
        }
    }

    for (const auto &endpoint : model.endpoints) {
        const bool owner_visible = endpoint.owner.is_valid() &&
                                   endpoint.owner.value() < visibility.entities.size() &&
                                   visibility.entities[endpoint.owner.value()];
        const bool network_space_visible =
            !endpoint.network_space.is_valid() ||
            (endpoint.network_space.value() < visibility.entities.size() &&
             visibility.entities[endpoint.network_space.value()]);
        visibility.endpoints[endpoint.id.value()] =
            owner_visible && network_space_visible &&
            (include_restricted || endpoint.sensitivity != Sensitivity::restricted);
        if (!visibility.endpoints[endpoint.id.value()]) {
            ++visibility.restricted_objects;
        }
    }
    return visibility;
}

[[nodiscard]] bool observation_visible(const NormalizedModel &model, const EvidenceIndex &evidence,
                                       const Visibility &visibility, std::size_t index) noexcept {
    if (index >= evidence.observations.size()) {
        return false;
    }
    const auto probe = evidence.observations[index].probe;
    if (!probe.is_valid() || probe.value() >= model.probes.size()) {
        return false;
    }
    const auto &contract = model.probes[probe.value()];
    return object_visible(visibility, contract.target) &&
           object_visible(visibility, contract.vantage);
}

[[nodiscard]] bool has_observation_detail(const Observation &observation) noexcept {
    return observation.duration_ms.has_value() || observation.value.has_value() ||
           observation.unit.has_value() || observation.state.has_value() ||
           observation.producer_sequence.has_value() || observation.producer_id.has_value() ||
           observation.evidence_ref.has_value() || observation.detail.has_value() ||
           !observation.extensions.empty();
}

[[nodiscard]] Omissions calculate_omissions(const NormalizedModel &model,
                                            const EvidenceIndex &evidence,
                                            const DiagnosisResult &diagnosis,
                                            const Visibility &visibility) {
    Omissions omissions;
    omissions.restricted_objects = visibility.restricted_objects;
    for (std::size_t index = 0; index < evidence.observations.size(); ++index) {
        if (!observation_visible(model, evidence, visibility, index)) {
            ++omissions.restricted_observations;
        }
        if (has_observation_detail(evidence.observations[index].observation)) {
            ++omissions.observation_details;
        }
    }
    for (const auto &finding : diagnosis.findings) {
        if (!valid_object(model, finding.hypothesis.target) ||
            !object_visible(visibility, finding.hypothesis.target)) {
            ++omissions.restricted_findings;
        }
    }
    return omissions;
}

[[nodiscard]] std::vector<std::string>
sorted_unique_strings(const std::vector<std::string> &values) {
    auto result = values;
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

[[nodiscard]] std::string escape_json(std::string_view value) {
    std::ostringstream output;
    output << '"';
    for (const unsigned char character : value) {
        switch (character) {
        case '"':
            output << "\\\"";
            break;
        case '\\':
            output << "\\\\";
            break;
        case '\b':
            output << "\\b";
            break;
        case '\f':
            output << "\\f";
            break;
        case '\n':
            output << "\\n";
            break;
        case '\r':
            output << "\\r";
            break;
        case '\t':
            output << "\\t";
            break;
        default:
            if (character < 0x20U) {
                output << "\\u00" << std::hex << std::setw(2) << std::setfill('0')
                       << static_cast<unsigned int>(character) << std::dec << std::setfill(' ');
            } else {
                output << static_cast<char>(character);
            }
        }
    }
    output << '"';
    return output.str();
}

[[nodiscard]] std::string escape_dot(std::string_view value) {
    std::ostringstream output;
    for (const unsigned char character : value) {
        switch (character) {
        case '"':
            output << "\\\"";
            break;
        case '\\':
            output << "\\\\";
            break;
        case '\n':
            output << "\\n";
            break;
        case '\r':
            output << "\\r";
            break;
        case '\t':
            output << "\\t";
            break;
        default:
            if (character < 0x20U) {
                output << "\\x" << std::hex << std::setw(2) << std::setfill('0')
                       << static_cast<unsigned int>(character) << std::dec << std::setfill(' ');
            } else {
                output << static_cast<char>(character);
            }
        }
    }
    return output.str();
}

void append_string_array(std::ostringstream &output, const std::vector<std::string> &values) {
    output << '[';
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        output << escape_json(values[index]);
    }
    output << ']';
}

void append_object(std::ostringstream &output, const NormalizedModel &model, ObjectRef object) {
    output << "{\"id\":" << escape_json(model.external_id(object))
           << ",\"kind\":" << escape_json(to_string(object.kind)) << '}';
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

[[nodiscard]] std::vector<ObjectRef> visible_sorted_objects(const NormalizedModel &model,
                                                            const Visibility &visibility,
                                                            std::vector<ObjectRef> objects) {
    objects.erase(std::remove_if(objects.begin(), objects.end(),
                                 [&](ObjectRef object) {
                                     return !valid_object(model, object) ||
                                            !object_visible(visibility, object);
                                 }),
                  objects.end());
    std::sort(objects.begin(), objects.end(),
              [&](ObjectRef left, ObjectRef right) { return object_less(model, left, right); });
    objects.erase(std::unique(objects.begin(), objects.end()), objects.end());
    return objects;
}

void append_object_array(std::ostringstream &output, const NormalizedModel &model,
                         const Visibility &visibility, std::vector<ObjectRef> objects) {
    objects = visible_sorted_objects(model, visibility, std::move(objects));
    output << '[';
    for (std::size_t index = 0; index < objects.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        append_object(output, model, objects[index]);
    }
    output << ']';
}

[[nodiscard]] std::vector<std::uint32_t>
visible_sorted_indices(const NormalizedModel &model, const EvidenceIndex &evidence,
                       const Visibility &visibility, std::vector<std::uint32_t> indices) {
    indices.erase(std::remove_if(indices.begin(), indices.end(),
                                 [&](std::uint32_t index) {
                                     return !observation_visible(model, evidence, visibility,
                                                                 index);
                                 }),
                  indices.end());
    std::sort(indices.begin(), indices.end());
    indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
    return indices;
}

void append_index_array(std::ostringstream &output, const NormalizedModel &model,
                        const EvidenceIndex &evidence, const Visibility &visibility,
                        std::vector<std::uint32_t> indices) {
    indices = visible_sorted_indices(model, evidence, visibility, std::move(indices));
    output << '[';
    for (std::size_t index = 0; index < indices.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        output << indices[index];
    }
    output << ']';
}

[[nodiscard]] std::string dot_object_node(ObjectRef object) {
    return "object:" + std::string{to_string(object.kind)} + ':' + std::to_string(object.index);
}

[[nodiscard]] std::string join_inputs(const std::vector<std::string> &values) {
    std::string result;
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) {
            result += ',';
        }
        result += values[index];
    }
    return result;
}

[[nodiscard]] std::string_view assessment_color(DiagnosisEvidenceAssessment value) noexcept {
    switch (value) {
    case DiagnosisEvidenceAssessment::supports_hypothesis:
        return "#15803d";
    case DiagnosisEvidenceAssessment::contradicts_hypothesis:
        return "#b91c1c";
    case DiagnosisEvidenceAssessment::consistent:
        return "#2563eb";
    case DiagnosisEvidenceAssessment::unexplained_non_pass:
        return "#c2410c";
    case DiagnosisEvidenceAssessment::unknown:
        return "#6b7280";
    }
    return "#6b7280";
}

} // namespace

std::string render_diagnosis_json(const NormalizedModel &model, const EvidenceIndex &evidence,
                                  const DiagnosisResult &diagnosis,
                                  const DiagnosisOutputContext &context) {
    const auto visibility = build_visibility(model, context.restricted_policy);
    const auto omissions = calculate_omissions(model, evidence, diagnosis, visibility);
    const auto model_inputs = sorted_unique_strings(context.model_inputs);
    const auto observation_inputs = sorted_unique_strings(context.observation_inputs);

    std::ostringstream output;
    output << "{\"schema\":" << escape_json(kDiagnosisSchema)
           << ",\"operation\":\"diagnose\",\"analysis_at\":"
           << escape_json(evidence.analysis_at.text) << ",\"model_inputs\":";
    append_string_array(output, model_inputs);
    output << ",\"observation_inputs\":";
    append_string_array(output, observation_inputs);
    output << ",\"status\":" << escape_json(to_string(diagnosis.status)) << ",\"evidence\":[";

    bool first = true;
    for (std::size_t index = 0; index < evidence.observations.size(); ++index) {
        if (!observation_visible(model, evidence, visibility, index)) {
            continue;
        }
        const auto &item = evidence.observations[index];
        const auto &probe = model.probes[item.probe.value()];
        if (!first) {
            output << ',';
        }
        first = false;
        output << "{\"index\":" << index
               << ",\"probe_id\":" << escape_json(model.string(probe.external_id))
               << ",\"target\":";
        append_object(output, model, probe.target);
        output << ",\"vantage\":";
        append_object(output, model, probe.vantage);
        output << ",\"kind\":" << escape_json(to_string(item.observation.kind))
               << ",\"outcome\":" << escape_json(to_string(item.observation.outcome))
               << ",\"observed_at\":" << escape_json(item.observation.observed_at.text)
               << ",\"valid_for_ms\":" << item.observation.valid_for_ms
               << ",\"freshness\":" << escape_json(to_string(item.freshness))
               << ",\"meaning\":" << escape_json(to_string(item.meaning))
               << ",\"conflicting\":" << (item.conflicting ? "true" : "false") << '}';
    }

    output << "],\"causal_cone\":";
    append_object_array(output, model, visibility, diagnosis.causal_cone);
    output << ",\"findings\":[";
    first = true;
    for (const auto &finding : diagnosis.findings) {
        if (!valid_object(model, finding.hypothesis.target) ||
            !object_visible(visibility, finding.hypothesis.target)) {
            continue;
        }
        if (!first) {
            output << ',';
        }
        first = false;
        output << "{\"rank\":" << finding.rank << ",\"tied\":" << (finding.tied ? "true" : "false")
               << ",\"hypothesis\":{\"id\":" << escape_json(finding.hypothesis.stable_id)
               << ",\"kind\":" << escape_json(to_string(finding.hypothesis.kind)) << ",\"target\":";
        append_object(output, model, finding.hypothesis.target);
        output << ",\"failure_mode\":";
        if (finding.hypothesis.kind == DiagnosisHypothesisKind::declared_failure_mode &&
            finding.hypothesis.failure_mode.is_valid() &&
            finding.hypothesis.failure_mode.value() < model.failure_modes.size()) {
            output << escape_json(model.string(
                model.failure_modes[finding.hypothesis.failure_mode.value()].external_id));
        } else {
            output << "null";
        }
        output << "},\"score\":{\"contradictions\":" << finding.contradictions
               << ",\"explained_non_pass\":" << finding.explained_non_pass
               << ",\"unexplained_non_pass\":" << finding.unexplained_non_pass
               << ",\"current_evidence\":" << finding.current_evidence
               << ",\"stale_evidence\":" << finding.stale_evidence
               << "},\"qualification\":{\"completeness\":"
               << escape_json(to_string(finding.qualification.completeness))
               << ",\"evidence_quality\":"
               << escape_json(to_string(finding.qualification.evidence_quality))
               << "},\"affected_objects\":";
        append_object_array(output, model, visibility, finding.affected_objects);
        output << ",\"evidence\":[";
        auto assessments = finding.evidence;
        std::sort(assessments.begin(), assessments.end(),
                  [](const DiagnosisEvidence &left, const DiagnosisEvidence &right) {
                      return std::tie(left.observation_index, left.assessment) <
                             std::tie(right.observation_index, right.assessment);
                  });
        bool first_assessment = true;
        for (const auto &assessment : assessments) {
            if (!observation_visible(model, evidence, visibility, assessment.observation_index)) {
                continue;
            }
            if (!first_assessment) {
                output << ',';
            }
            first_assessment = false;
            output << "{\"observation_index\":" << assessment.observation_index
                   << ",\"assessment\":" << escape_json(to_string(assessment.assessment)) << '}';
        }
        output << "],\"conflicting_evidence\":";
        append_index_array(output, model, evidence, visibility, finding.conflicting_evidence);
        output << ",\"remaining_unknown_objects\":";
        append_object_array(output, model, visibility, finding.remaining_unknown_objects);
        output << ",\"remaining_unknown_evidence\":";
        append_index_array(output, model, evidence, visibility, finding.remaining_unknown_evidence);
        output << '}';
    }

    output << "],\"incomplete_reasons\":[";
    for (std::size_t index = 0; index < diagnosis.incomplete_reasons.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        output << escape_json(to_string(diagnosis.incomplete_reasons[index]));
    }
    output << "],\"warnings\":";
    if (omissions.restricted_objects == 0 && omissions.restricted_observations == 0 &&
        omissions.restricted_findings == 0 && omissions.observation_details == 0) {
        output << "[]";
    } else {
        output << "[\"content omitted by output policy\"]";
    }
    output << ",\"resource_budget_usage\":{\"causal_objects\":" << diagnosis.usage.causal_objects
           << ",\"candidate_hypotheses\":" << diagnosis.usage.candidate_hypotheses
           << ",\"generated_hypotheses\":" << diagnosis.usage.generated_hypotheses
           << ",\"simulations\":" << diagnosis.usage.simulations
           << ",\"evidence_comparisons\":" << diagnosis.usage.evidence_comparisons
           << "},\"omissions\":{\"restricted_objects\":" << omissions.restricted_objects
           << ",\"restricted_observations\":" << omissions.restricted_observations
           << ",\"restricted_findings\":" << omissions.restricted_findings
           << ",\"observation_details\":" << omissions.observation_details << "}}\n";
    return output.str();
}

std::string render_diagnosis_dot(const NormalizedModel &model, const EvidenceIndex &evidence,
                                 const DiagnosisResult &diagnosis,
                                 const DiagnosisOutputContext &context,
                                 const DiagnosisDotOutputOptions &options) {
    const auto visibility = build_visibility(model, context.restricted_policy);
    const auto omissions = calculate_omissions(model, evidence, diagnosis, visibility);
    const auto model_inputs = sorted_unique_strings(context.model_inputs);
    const auto observation_inputs = sorted_unique_strings(context.observation_inputs);

    std::ostringstream output;
    output << "digraph ltfa_diagnosis {\n"
           << "  graph [ltfa_schema=\"" << kDiagnosisDotSchema
           << "\",operation=\"diagnose\",analysis_at=\"" << escape_dot(evidence.analysis_at.text)
           << "\",status=\"" << escape_dot(to_string(diagnosis.status)) << "\",model_inputs=\""
           << escape_dot(join_inputs(model_inputs)) << "\",observation_inputs=\""
           << escape_dot(join_inputs(observation_inputs)) << "\",restricted_omissions=\""
           << omissions.restricted_objects + omissions.restricted_observations +
                  omissions.restricted_findings
           << "\",observation_detail_omissions=\"" << omissions.observation_details << "\"];\n"
           << "  rankdir=LR;\n"
           << "  node [fontname=\"sans-serif\"];\n";

    if (options.finding_index >= diagnosis.findings.size()) {
        output << "}\n";
        return output.str();
    }
    const auto &finding = diagnosis.findings[options.finding_index];
    if (!valid_object(model, finding.hypothesis.target) ||
        !object_visible(visibility, finding.hypothesis.target)) {
        output << "}\n";
        return output.str();
    }

    std::string hypothesis_label =
        finding.hypothesis.stable_id + '\n' + std::string{to_string(finding.hypothesis.kind)} +
        " rank=" + std::to_string(finding.rank) + '\n' +
        std::string{to_string(finding.qualification.completeness)} + '/' +
        std::string{to_string(finding.qualification.evidence_quality)};
    output << "  \"hypothesis\" [shape=box,style=\"rounded,filled\",fillcolor=\"#fee2e2\","
              "label=\""
           << escape_dot(hypothesis_label) << "\"];\n";

    auto affected = visible_sorted_objects(model, visibility, finding.affected_objects);
    auto unknowns = visible_sorted_objects(model, visibility, finding.remaining_unknown_objects);
    std::vector<ObjectRef> objects = affected;
    objects.insert(objects.end(), unknowns.begin(), unknowns.end());
    objects.push_back(finding.hypothesis.target);
    objects = visible_sorted_objects(model, visibility, std::move(objects));
    for (const auto object : objects) {
        const bool unknown = std::find(unknowns.begin(), unknowns.end(), object) != unknowns.end();
        const bool target = object == finding.hypothesis.target;
        output << "  \"" << dot_object_node(object) << "\" [shape=ellipse,label=\""
               << escape_dot(model.external_id(object)) << "\",style=\""
               << (unknown  ? "dotted"
                   : target ? "filled"
                            : "solid")
               << "\"";
        if (target) {
            output << ",fillcolor=\"#fef3c7\"";
        }
        output << "];\n";
    }
    output << "  \"hypothesis\" -> \"" << dot_object_node(finding.hypothesis.target)
           << "\" [label=\"targets\",color=\"#b91c1c\"];\n";
    for (const auto object : affected) {
        if (object != finding.hypothesis.target) {
            output << "  \"hypothesis\" -> \"" << dot_object_node(object)
                   << "\" [label=\"affects\",color=\"#c2410c\"];\n";
        }
    }
    for (const auto object : unknowns) {
        if (object != finding.hypothesis.target) {
            output << "  \"hypothesis\" -> \"" << dot_object_node(object)
                   << "\" [label=\"unknown\",color=\"#6b7280\",style=\"dotted\"];\n";
        }
    }

    auto assessments = finding.evidence;
    std::sort(assessments.begin(), assessments.end(),
              [](const DiagnosisEvidence &left, const DiagnosisEvidence &right) {
                  return std::tie(left.observation_index, left.assessment) <
                         std::tie(right.observation_index, right.assessment);
              });
    std::set<std::uint32_t> visible_assessments;
    const auto conflicts =
        visible_sorted_indices(model, evidence, visibility, finding.conflicting_evidence);
    for (const auto &assessment : assessments) {
        if (!observation_visible(model, evidence, visibility, assessment.observation_index)) {
            continue;
        }
        visible_assessments.insert(assessment.observation_index);
        const auto &item = evidence.observations[assessment.observation_index];
        const auto &probe = model.probes[item.probe.value()];
        const bool conflicting =
            std::binary_search(conflicts.begin(), conflicts.end(), assessment.observation_index);
        const std::string label = "observation " + std::to_string(assessment.observation_index) +
                                  '\n' + std::string{model.string(probe.external_id)} + '\n' +
                                  std::string{to_string(item.observation.outcome)} + '/' +
                                  std::string{to_string(item.freshness)} + '\n' +
                                  std::string{model.external_id(probe.vantage)};
        output << "  \"observation:" << assessment.observation_index << "\" [shape=note,label=\""
               << escape_dot(label) << "\",color=\"" << (conflicting ? "#d97706" : "#6b7280")
               << "\",penwidth=\"" << (conflicting ? "2" : "1") << "\"];\n"
               << "  \"observation:" << assessment.observation_index
               << "\" -> \"hypothesis\" [label=\"" << escape_dot(to_string(assessment.assessment))
               << "\",color=\"" << assessment_color(assessment.assessment) << "\"];\n";
    }

    for (const auto &conflict : evidence.conflicts) {
        auto indices =
            visible_sorted_indices(model, evidence, visibility, conflict.observation_indices);
        indices.erase(std::remove_if(indices.begin(), indices.end(),
                                     [&](std::uint32_t index) {
                                         return visible_assessments.find(index) ==
                                                visible_assessments.end();
                                     }),
                      indices.end());
        for (std::size_t index = 1; index < indices.size(); ++index) {
            output << "  \"observation:" << indices.front()
                   << "\" -> \"observation:" << indices[index] << "\" [dir=none,label=\""
                   << escape_dot(to_string(conflict.kind))
                   << "\",color=\"#d97706\",style=\"dashed\"];\n";
        }
    }
    output << "}\n";
    return output.str();
}

} // namespace ltfa
