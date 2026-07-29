#include "assurance_output.hpp"

#include <ostream>
#include <sstream>
#include <string_view>

namespace ltfa::app {
namespace {

void json_string(std::ostream &output, std::string_view value) {
    constexpr char kHexDigits[] = "0123456789abcdef";
    output << '"';
    for (const unsigned char byte : value) {
        switch (byte) {
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
            if (byte < 0x20U) {
                output << "\\u00" << kHexDigits[(byte >> 4U) & 0x0fU] << kHexDigits[byte & 0x0fU];
            } else {
                output << static_cast<char>(byte);
            }
            break;
        }
    }
    output << '"';
}

std::string_view analysis_status(AnalysisStatus status) noexcept {
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

std::string_view propagation_status(PropagationStatus status) noexcept {
    switch (status) {
    case PropagationStatus::complete:
        return "complete";
    case PropagationStatus::incomplete:
        return "incomplete";
    case PropagationStatus::invalid:
        return "invalid";
    }
    return "invalid";
}

std::string_view removal_kind(RemovalKind kind) noexcept {
    switch (kind) {
    case RemovalKind::object:
        return "object";
    case RemovalKind::relation:
        return "relation";
    case RemovalKind::failure_domain:
        return "failure-domain";
    }
    return "object";
}

std::string_view candidate_id(const NormalizedModel &model, RemovalTarget candidate) {
    switch (candidate.kind) {
    case RemovalKind::object:
        return model.external_id(candidate.object_value);
    case RemovalKind::relation:
        return model.string(model.relations[candidate.relation_value.value()].external_id);
    case RemovalKind::failure_domain:
        return model.string(
            model.failure_domains[candidate.failure_domain_value.value()].external_id);
    }
    return {};
}

void human_objects(std::ostream &output, std::string_view name, const NormalizedModel &model,
                   const std::vector<ObjectRef> &objects, std::string_view indent = "  ") {
    output << indent << name << ":\n";
    if (objects.empty()) {
        output << indent << "  - none\n";
        return;
    }
    for (const auto object : objects) {
        output << indent << "  - " << model.external_id(object) << '\n';
    }
}

void human_relations(std::ostream &output, std::string_view name, const NormalizedModel &model,
                     const std::vector<RelationId> &relations, std::string_view indent = "  ") {
    output << indent << name << ":\n";
    if (relations.empty()) {
        output << indent << "  - none\n";
        return;
    }
    for (const auto relation : relations) {
        output << indent << "  - " << model.string(model.relations[relation.value()].external_id)
               << '\n';
    }
}

void human_domains(std::ostream &output, std::string_view name, const NormalizedModel &model,
                   const std::vector<FailureDomainId> &domains,
                   std::string_view indent = "      ") {
    output << indent << name << ":\n";
    if (domains.empty()) {
        output << indent << "  - none\n";
        return;
    }
    for (const auto domain : domains) {
        output << indent << "  - "
               << model.string(model.failure_domains[domain.value()].external_id) << '\n';
    }
}

void json_objects(std::ostream &output, const NormalizedModel &model,
                  const std::vector<ObjectRef> &objects) {
    output << '[';
    for (std::size_t index = 0; index < objects.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        json_string(output, model.external_id(objects[index]));
    }
    output << ']';
}

void json_relations(std::ostream &output, const NormalizedModel &model,
                    const std::vector<RelationId> &relations) {
    output << '[';
    for (std::size_t index = 0; index < relations.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        json_string(output, model.string(model.relations[relations[index].value()].external_id));
    }
    output << ']';
}

void json_domains(std::ostream &output, const NormalizedModel &model,
                  const std::vector<FailureDomainId> &domains) {
    output << '[';
    for (std::size_t index = 0; index < domains.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        json_string(output,
                    model.string(model.failure_domains[domains[index].value()].external_id));
    }
    output << ']';
}

void json_qualification(std::ostream &output, AssuranceQualification qualification) {
    output << "{\"completeness\":";
    json_string(output, to_string(qualification.completeness));
    output << ",\"evidence_quality\":";
    json_string(output, to_string(qualification.evidence_quality));
    output << '}';
}

void json_spof_path(std::ostream &output, const NormalizedModel &model,
                    const SpofPathEvidence &path) {
    output << "{\"target\":";
    json_string(output, model.external_id(path.target));
    output << ",\"objects\":";
    json_objects(output, model, path.objects);
    output << ",\"relations\":";
    json_relations(output, model, path.relations);
    output << '}';
}

void json_spof_paths(std::ostream &output, const NormalizedModel &model,
                     const std::vector<SpofPathEvidence> &paths) {
    output << '[';
    for (std::size_t index = 0; index < paths.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        json_spof_path(output, model, paths[index]);
    }
    output << ']';
}

void json_propagation_objects(std::ostream &output, const NormalizedModel &model,
                              const PropagationResult &result) {
    output << "{\"immediate_unavailable\":";
    json_objects(output, model, result.immediate_unavailable);
    output << ",\"immediate_degraded\":";
    json_objects(output, model, result.immediate_degraded);
    output << ",\"delayed_unavailable\":";
    json_objects(output, model, result.delayed_unavailable);
    output << ",\"delayed_degraded\":";
    json_objects(output, model, result.delayed_degraded);
    output << ",\"unaffected\":";
    json_objects(output, model, result.unaffected_objects);
    output << ",\"unknown\":";
    json_objects(output, model, result.unknown_objects);
    output << ",\"detection_gaps\":";
    json_objects(output, model, result.detection_gaps);
    output << '}';
}

} // namespace

std::string render_assurance_human(const NormalizedModel &model, const AssuranceReport &report) {
    std::ostringstream output;
    output << "result: " << (report.incomplete ? "incomplete" : "complete") << '\n'
           << "schema: ltfa.assurance/v1\n"
           << "operation: analyze\n"
           << "focus: " << model.external_id(report.source) << " -> "
           << model.external_id(report.target) << '\n'
           << "scope: " << model.string(model.scopes[report.scope.value()].external_id) << '\n'
           << "restricted-output: " << (report.include_restricted ? "include-explicit" : "omit")
           << '\n'
           << "model-inputs:\n";
    for (const auto &input : report.model_inputs) {
        output << "  - " << input << '\n';
    }
    output << "path:\n"
           << "  status: " << analysis_status(report.path.status) << '\n'
           << "  reachable: " << (report.path.reachable ? "true" : "false") << '\n'
           << "  completeness: " << to_string(report.path.qualification.completeness) << '\n'
           << "  evidence-quality: " << to_string(report.path.qualification.evidence_quality)
           << '\n'
           << "  contains-unknown: "
           << (report.path.contains_unknown_availability ? "true" : "false") << '\n';
    human_objects(output, "objects", model, report.path.objects);
    human_relations(output, "relations", model, report.path.relations);
    output << "  usage: visited-objects=" << report.path.usage.visited_objects
           << " examined-relations=" << report.path.usage.examined_relations << '\n';

    if (!report.spof.has_value()) {
        output << "spof:\n"
               << "  status: not-applicable\n"
               << "  reason: "
               << (report.path.status == AnalysisStatus::incomplete
                       ? "path analysis stopped at its work budget"
                       : "target is not reachable in the selected view")
               << '\n';
    } else {
        const auto &spof = *report.spof;
        output << "structural:\n"
               << "  status: " << analysis_status(spof.structural.status) << '\n'
               << "  completeness: " << to_string(spof.structural.qualification.completeness)
               << '\n'
               << "  evidence-quality: "
               << to_string(spof.structural.qualification.evidence_quality) << '\n'
               << "  contains-unknown: "
               << (spof.structural.contains_unknown_availability ? "true" : "false") << '\n'
               << "  alternatives-excluded-by-filter: "
               << (spof.structural.alternatives_excluded_by_filter ? "true" : "false") << '\n';
        human_objects(output, "articulation-points", model, spof.structural.articulation_points);
        human_relations(output, "bridges", model, spof.structural.bridges);
        output << "spof:\n"
               << "  status: " << analysis_status(spof.status) << '\n'
               << "  findings:\n";
        if (spof.findings.empty()) {
            output << "    - none\n";
        }
        for (const auto &finding : spof.findings) {
            output << "    - candidate: " << removal_kind(finding.candidate.kind) << ':'
                   << candidate_id(model, finding.candidate) << '\n'
                   << "      classification: " << to_string(finding.classification) << '\n';
            human_objects(output, "affected-targets", model, finding.affected_targets, "      ");
            human_objects(output, "affected-objects", model, finding.affected_objects, "      ");
            human_objects(output, "common-cause-members", model, finding.common_cause_members,
                          "      ");
            human_domains(output, "common-domains", model, finding.common_domains);
            human_objects(output, "remaining-unknowns", model, finding.remaining_unknowns,
                          "      ");
            output << "      retained-paths: " << finding.retained_paths.size() << '\n';
        }
        output << "  usage: candidates-considered=" << spof.usage.candidates_considered
               << " visited-objects=" << spof.usage.visited_objects
               << " examined-relations=" << spof.usage.examined_relations << '\n';
    }

    output << "propagation:\n";
    if (!report.propagation.has_value()) {
        output << "  status: not-requested\n";
    } else {
        const auto &propagation = *report.propagation;
        output << "  status: " << propagation_status(propagation.status) << '\n'
               << "  failure: " << *report.failure_external_id << '\n'
               << "  completeness: " << to_string(propagation.qualification.completeness) << '\n'
               << "  evidence-quality: " << to_string(propagation.qualification.evidence_quality)
               << '\n'
               << "  events:\n";
        if (propagation.events.empty()) {
            output << "    - none\n";
        }
        for (const auto &event : propagation.events) {
            output << "    - at-ms=" << event.effective_time_ms
                   << " object=" << model.external_id(event.object)
                   << " state=" << to_string(event.state) << '\n';
        }
        human_objects(output, "immediate-unavailable", model, propagation.immediate_unavailable);
        human_objects(output, "immediate-degraded", model, propagation.immediate_degraded);
        human_objects(output, "delayed-unavailable", model, propagation.delayed_unavailable);
        human_objects(output, "delayed-degraded", model, propagation.delayed_degraded);
        human_objects(output, "unknown", model, propagation.unknown_objects);
        human_objects(output, "detection-gaps", model, propagation.detection_gaps);
        output << "  usage: scheduled-events=" << propagation.usage.scheduled_events
               << " processed-events=" << propagation.usage.processed_events
               << " state-transitions=" << propagation.usage.state_transitions << '\n';
    }

    output << "budgets:\n"
           << "  max-candidates: " << report.budgets.spof.max_candidates << '\n'
           << "  max-visited-objects: " << report.budgets.spof.max_total_visited_objects << '\n'
           << "  max-examined-relations: " << report.budgets.spof.max_total_examined_relations
           << '\n'
           << "  max-events: " << report.budgets.propagation.max_events << '\n'
           << "  max-state-transitions: " << report.budgets.propagation.max_state_transitions
           << '\n';
    return output.str();
}

std::string render_assurance_json(const NormalizedModel &model, const AssuranceReport &report) {
    std::ostringstream output;
    output << "{\"schema\":\"ltfa.assurance/v1\",\"operation\":\"analyze\",\"result\":";
    json_string(output, report.incomplete ? "incomplete" : "complete");
    output << ",\"scope\":";
    json_string(output, model.string(model.scopes[report.scope.value()].external_id));
    output << ",\"source\":";
    json_string(output, model.external_id(report.source));
    output << ",\"target\":";
    json_string(output, model.external_id(report.target));
    output << ",\"restricted_output\":";
    json_string(output, report.include_restricted ? "include-explicit" : "omit");
    output << ",\"model_inputs\":[";
    for (std::size_t index = 0; index < report.model_inputs.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        json_string(output, report.model_inputs[index]);
    }
    output << "],\"path\":{\"status\":";
    json_string(output, analysis_status(report.path.status));
    output << ",\"reachable\":" << (report.path.reachable ? "true" : "false")
           << ",\"qualification\":";
    json_qualification(output, report.path.qualification);
    output << ",\"contains_unknown_availability\":"
           << (report.path.contains_unknown_availability ? "true" : "false") << ",\"objects\":";
    json_objects(output, model, report.path.objects);
    output << ",\"relations\":";
    json_relations(output, model, report.path.relations);
    output << ",\"usage\":{\"visited_objects\":" << report.path.usage.visited_objects
           << ",\"examined_relations\":" << report.path.usage.examined_relations << "}}";

    output << ",\"structural\":";
    if (!report.spof.has_value()) {
        output << "null";
    } else {
        const auto &structural = report.spof->structural;
        output << "{\"status\":";
        json_string(output, analysis_status(structural.status));
        output << ",\"qualification\":";
        json_qualification(output, structural.qualification);
        output << ",\"contains_unknown_availability\":"
               << (structural.contains_unknown_availability ? "true" : "false")
               << ",\"alternatives_excluded_by_filter\":"
               << (structural.alternatives_excluded_by_filter ? "true" : "false")
               << ",\"articulation_points\":";
        json_objects(output, model, structural.articulation_points);
        output << ",\"bridges\":";
        json_relations(output, model, structural.bridges);
        output << ",\"usage\":{\"visited_objects\":" << structural.usage.visited_objects
               << ",\"examined_relations\":" << structural.usage.examined_relations << "}}";
    }

    output << ",\"spof\":";
    if (!report.spof.has_value()) {
        output << "{\"status\":\"not-applicable\",\"reason\":";
        json_string(output, report.path.status == AnalysisStatus::incomplete
                                ? "path-incomplete"
                                : "target-unreachable");
        output << ","
                  "\"findings\":[]}";
    } else {
        const auto &spof = *report.spof;
        output << "{\"status\":";
        json_string(output, analysis_status(spof.status));
        output << ",\"findings\":[";
        for (std::size_t index = 0; index < spof.findings.size(); ++index) {
            if (index != 0) {
                output << ',';
            }
            const auto &finding = spof.findings[index];
            output << "{\"candidate\":{\"kind\":";
            json_string(output, removal_kind(finding.candidate.kind));
            output << ",\"id\":";
            json_string(output, candidate_id(model, finding.candidate));
            output << "},\"classification\":";
            json_string(output, to_string(finding.classification));
            output << ",\"affected_targets\":";
            json_objects(output, model, finding.affected_targets);
            output << ",\"affected_objects\":";
            json_objects(output, model, finding.affected_objects);
            output << ",\"common_cause_members\":";
            json_objects(output, model, finding.common_cause_members);
            output << ",\"common_domains\":";
            json_domains(output, model, finding.common_domains);
            output << ",\"baseline_paths\":";
            json_spof_paths(output, model, finding.baseline_paths);
            output << ",\"retained_paths\":";
            json_spof_paths(output, model, finding.retained_paths);
            output << ",\"remaining_unknowns\":";
            json_objects(output, model, finding.remaining_unknowns);
            output << ",\"qualification\":";
            json_qualification(output, finding.qualification);
            output << ",\"alternatives_excluded_by_filter\":"
                   << (finding.alternatives_excluded_by_filter ? "true" : "false") << '}';
        }
        output << "],\"usage\":{\"candidates_considered\":" << spof.usage.candidates_considered
               << ",\"visited_objects\":" << spof.usage.visited_objects
               << ",\"examined_relations\":" << spof.usage.examined_relations << "}}";
    }

    output << ",\"propagation\":";
    if (!report.propagation.has_value()) {
        output << "null";
    } else {
        const auto &propagation = *report.propagation;
        output << "{\"status\":";
        json_string(output, propagation_status(propagation.status));
        output << ",\"failure\":";
        json_string(output, *report.failure_external_id);
        output << ",\"qualification\":";
        json_qualification(output, propagation.qualification);
        output << ",\"unresolved_failure_mode\":"
               << (propagation.unresolved_failure_mode ? "true" : "false") << ",\"events\":[";
        for (std::size_t index = 0; index < propagation.events.size(); ++index) {
            if (index != 0) {
                output << ',';
            }
            const auto &event = propagation.events[index];
            output << "{\"effective_time_ms\":" << event.effective_time_ms << ",\"object\":";
            json_string(output, model.external_id(event.object));
            output << ",\"previous_state\":";
            json_string(output, to_string(event.previous_state));
            output << ",\"state\":";
            json_string(output, to_string(event.state));
            output << ",\"qualification\":";
            json_qualification(output, event.qualification);
            output << '}';
        }
        output << "],\"objects\":";
        json_propagation_objects(output, model, propagation);
        output << ",\"usage\":{\"scheduled_events\":" << propagation.usage.scheduled_events
               << ",\"processed_events\":" << propagation.usage.processed_events
               << ",\"state_transitions\":" << propagation.usage.state_transitions << "}}";
    }

    output << ",\"budgets\":{\"max_candidates\":" << report.budgets.spof.max_candidates
           << ",\"max_visited_objects\":" << report.budgets.spof.max_total_visited_objects
           << ",\"max_examined_relations\":" << report.budgets.spof.max_total_examined_relations
           << ",\"max_events\":" << report.budgets.propagation.max_events
           << ",\"max_state_transitions\":" << report.budgets.propagation.max_state_transitions
           << "}}\n";
    return output.str();
}

} // namespace ltfa::app
