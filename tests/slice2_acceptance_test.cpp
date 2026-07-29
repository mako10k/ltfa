#include "ltfa/dsl/include_resolver.hpp"
#include "ltfa/failure_propagation.hpp"
#include "ltfa/model_compiler.hpp"
#include "ltfa/model_output.hpp"
#include "ltfa/path_analysis.hpp"
#include "ltfa/spof_analysis.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#ifndef LTFA_SLICE2_FIXTURE_ROOT
#error "LTFA_SLICE2_FIXTURE_ROOT must name the Slice 2 fixture directory"
#endif

namespace {

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

ltfa::NormalizedModel compile_fixture() {
    const auto loaded =
        ltfa::dsl::load_document_tree(std::filesystem::path{LTFA_SLICE2_FIXTURE_ROOT}, "main.ltfa");
    if (loaded.error.has_value()) {
        std::cerr << ltfa::to_string(loaded.error->code) << ": " << loaded.error->message << '\n';
    }
    expect(loaded.ok(), "Slice 2 fixture must load");
    const auto compiled = ltfa::compile_model(*loaded.document);
    if (compiled.error.has_value()) {
        std::cerr << ltfa::to_string(compiled.error->code) << ": " << compiled.error->message
                  << '\n';
    }
    expect(compiled.ok(), "Slice 2 fixture must compile");
    return *compiled.model;
}

ltfa::ObjectRef find_object(const ltfa::NormalizedModel &model, std::string_view external_id) {
    for (const auto &entity : model.entities) {
        if (model.string(entity.external_id) == external_id) {
            return ltfa::ObjectRef::from(entity.id);
        }
    }
    return {};
}

ltfa::RelationId find_relation(const ltfa::NormalizedModel &model, std::string_view from,
                               std::string_view to) {
    for (const auto &relation : model.relations) {
        if (model.external_id(relation.from) == from && model.external_id(relation.to) == to) {
            return relation.id;
        }
    }
    return {};
}

ltfa::FailureDomainId find_domain(const ltfa::NormalizedModel &model,
                                  std::string_view external_id) {
    for (const auto &domain : model.failure_domains) {
        if (model.string(domain.external_id) == external_id) {
            return domain.id;
        }
    }
    return {};
}

ltfa::FailureModeId find_failure(const ltfa::NormalizedModel &model, std::string_view external_id) {
    for (const auto &failure : model.failure_modes) {
        if (model.string(failure.external_id) == external_id) {
            return failure.id;
        }
    }
    return {};
}

std::vector<std::string_view> object_ids(const ltfa::NormalizedModel &model,
                                         const std::vector<ltfa::ObjectRef> &objects) {
    std::vector<std::string_view> result;
    for (const auto object : objects) {
        result.push_back(model.external_id(object));
    }
    return result;
}

const ltfa::SpofFinding *find_finding(const ltfa::SpofAnalysisResult &result,
                                      ltfa::RemovalKind kind, std::string_view external_id,
                                      const ltfa::NormalizedModel &model) {
    for (const auto &finding : result.findings) {
        if (finding.candidate.kind != kind) {
            continue;
        }
        switch (kind) {
        case ltfa::RemovalKind::object:
            if (model.external_id(finding.candidate.object_value) == external_id) {
                return &finding;
            }
            break;
        case ltfa::RemovalKind::relation:
            if (finding.candidate.relation_value.is_valid() &&
                model.string(
                    model.relations[finding.candidate.relation_value.value()].external_id) ==
                    external_id) {
                return &finding;
            }
            break;
        case ltfa::RemovalKind::failure_domain:
            if (finding.candidate.failure_domain_value.is_valid() &&
                model.string(model.failure_domains[finding.candidate.failure_domain_value.value()]
                                 .external_id) == external_id) {
                return &finding;
            }
            break;
        }
    }
    return nullptr;
}

ltfa::GraphFilter communication_view(const ltfa::NormalizedModel &model,
                                     std::string_view scope_id) {
    ltfa::GraphFilter filter;
    filter.plane = ltfa::Plane::communication;
    filter.layer = ltfa::Layer::physical;
    filter.relation_kinds = {ltfa::RelationKind::depends_on};
    filter.direction = ltfa::TraversalDirection::undirected;
    for (const auto &scope : model.scopes) {
        if (model.string(scope.external_id) == scope_id) {
            filter.scope = scope.id;
        }
    }
    expect(filter.scope.has_value(), "acceptance view scope must exist");
    return filter;
}

std::string candidate_id(const ltfa::NormalizedModel &model, ltfa::RemovalTarget candidate) {
    switch (candidate.kind) {
    case ltfa::RemovalKind::object:
        return std::string{model.external_id(candidate.object_value)};
    case ltfa::RemovalKind::relation:
        return std::string{
            model.string(model.relations[candidate.relation_value.value()].external_id)};
    case ltfa::RemovalKind::failure_domain:
        return std::string{model.string(
            model.failure_domains[candidate.failure_domain_value.value()].external_id)};
    }
    return {};
}

std::vector<std::string> spof_facts(const ltfa::NormalizedModel &model,
                                    const ltfa::SpofAnalysisResult &result) {
    std::vector<std::string> facts;
    for (const auto &finding : result.findings) {
        std::string fact = candidate_id(model, finding.candidate) + ":" +
                           std::string{ltfa::to_string(finding.classification)} + ":";
        for (const auto target : finding.affected_targets) {
            fact += std::string{model.external_id(target)} + ",";
        }
        facts.push_back(std::move(fact));
    }
    return facts;
}

std::vector<std::string> event_facts(const ltfa::NormalizedModel &model,
                                     const ltfa::PropagationResult &result) {
    std::vector<std::string> facts;
    for (const auto &event : result.events) {
        facts.push_back(std::to_string(event.effective_time_ms) + ":" +
                        std::string{model.external_id(event.object)} + ":" +
                        std::string{ltfa::to_string(event.state)});
    }
    return facts;
}

void test_verified_redundancy_and_common_cause(const ltfa::NormalizedModel &model) {
    const auto source = find_object(model, "device:path-source");
    const auto target = find_object(model, "device:path-target");
    const auto tail = find_object(model, "device:path-tail");
    const auto shared_route = find_domain(model, "shared-route");
    const auto source_a = find_relation(model, "device:path-source", "device:path-a");
    const auto snapshot = ltfa::make_availability_snapshot(model);
    const auto view = communication_view(model, "site:verified");

    ltfa::SpofRequest request;
    request.source = source;
    request.targets = {tail};
    request.view = view;
    const auto automatic = ltfa::analyze_spof(model, snapshot, request);
    expect(automatic.status == ltfa::AnalysisStatus::complete && automatic.findings.size() == 3,
           "verified fixture must expose exactly three target-affecting candidates");

    const auto *articulation =
        find_finding(automatic, ltfa::RemovalKind::object, "device:path-target", model);
    const auto *common_cause =
        find_finding(automatic, ltfa::RemovalKind::failure_domain, "shared-route", model);
    expect(articulation != nullptr && articulation->candidate.object_value == target &&
               articulation->classification == ltfa::SpofClassification::confirmed_spof,
           "verified articulation must be a confirmed SPOF");
    expect(common_cause != nullptr &&
               common_cause->candidate.failure_domain_value == shared_route &&
               common_cause->classification == ltfa::SpofClassification::confirmed_spof &&
               object_ids(model, common_cause->common_cause_members) ==
                   std::vector<std::string_view>{"device:path-a", "device:path-b"},
           "shared route must expose the common-cause risk");

    request.candidates = {ltfa::RemovalTarget::for_relation(source_a)};
    const auto redundant = ltfa::analyze_spof(model, snapshot, request);
    expect(redundant.status == ltfa::AnalysisStatus::complete && redundant.findings.size() == 1 &&
               redundant.findings.front().classification ==
                   ltfa::SpofClassification::not_a_spof_in_view &&
               redundant.findings.front().affected_targets.empty() &&
               redundant.findings.front().retained_paths.size() == 1,
           "one redundant link must retain an evidenced path");

    const auto path = ltfa::find_path(model, snapshot, source, tail, view);
    expect(path.status == ltfa::AnalysisStatus::complete && path.reachable &&
               path.objects.front() == source && path.objects.back() == tail,
           "the verified service path must be reachable in the selected view");
}

void test_breaker_and_fixed_delay(const ltfa::NormalizedModel &model) {
    const auto snapshot = ltfa::make_availability_snapshot(model);
    const auto breaker = find_failure(model, "failure:breaker");
    const auto result = ltfa::propagate_failure(model, snapshot,
                                                ltfa::FailureInjection::for_declared_mode(breaker));

    expect(result.status == ltfa::PropagationStatus::complete &&
               event_facts(model, result) ==
                   std::vector<std::string>{
                       "0:facility:breaker:unavailable", "0:service:immediate:unavailable",
                       "30000:service:held:unavailable", "30000:service:held-child:unavailable"},
           "breaker failure must retain exact immediate and delayed effects");
    expect(result.final_snapshot.state(find_object(model, "service:redundant")) ==
               ltfa::AvailabilityState::available,
           "independent backup power must retain the redundant service");

    const auto shared_power = find_domain(model, "shared-power");
    const auto common_cause = ltfa::propagate_failure(
        model, snapshot, ltfa::FailureInjection::for_failure_domain(shared_power));
    expect(common_cause.status == ltfa::PropagationStatus::complete &&
               common_cause.final_snapshot.state(find_object(model, "service:redundant")) ==
                   ltfa::AvailabilityState::unavailable,
           "shared power-domain failure must remove both modeled providers");
}

void test_partial_model_qualification(const ltfa::NormalizedModel &model) {
    const auto source = find_object(model, "device:partial-source");
    const auto tail = find_object(model, "device:partial-tail");
    const auto source_a = find_relation(model, "device:partial-source", "device:partial-a");
    const auto snapshot = ltfa::make_availability_snapshot(model);

    ltfa::SpofRequest request;
    request.source = source;
    request.targets = {tail};
    request.view = communication_view(model, "site:partial");
    const auto result = ltfa::analyze_spof(model, snapshot, request);
    expect(result.status == ltfa::AnalysisStatus::complete && result.findings.size() == 3,
           "partial fixture must retain modeled target-affecting findings");
    for (const auto &finding : result.findings) {
        expect(finding.classification == ltfa::SpofClassification::model_spof &&
                   finding.qualification.completeness == ltfa::Completeness::partial &&
                   finding.qualification.evidence_quality == ltfa::EvidenceQuality::inferred,
               "partial topology must never produce an unqualified confirmed SPOF");
    }

    request.candidates = {ltfa::RemovalTarget::for_relation(source_a)};
    const auto redundant = ltfa::analyze_spof(model, snapshot, request);
    expect(redundant.findings.size() == 1 &&
               redundant.findings.front().classification ==
                   ltfa::SpofClassification::not_a_spof_in_view &&
               redundant.findings.front().qualification.completeness == ltfa::Completeness::partial,
           "partial redundancy must remain explicitly limited to the selected view");
}

void test_determinism_and_budgets(const ltfa::NormalizedModel &model) {
    const auto snapshot = ltfa::make_availability_snapshot(model);
    ltfa::SpofRequest request;
    request.source = find_object(model, "device:path-source");
    request.targets = {find_object(model, "device:path-tail")};
    request.view = communication_view(model, "site:verified");
    const auto first_spof = ltfa::analyze_spof(model, snapshot, request);
    const auto second_spof = ltfa::analyze_spof(model, snapshot, request);
    expect(spof_facts(model, first_spof) == spof_facts(model, second_spof) &&
               first_spof.usage.candidates_considered == second_spof.usage.candidates_considered &&
               first_spof.usage.visited_objects == second_spof.usage.visited_objects &&
               first_spof.usage.examined_relations == second_spof.usage.examined_relations,
           "identical SPOF analysis and budgets must produce stable ordered facts and usage");

    const auto breaker = find_failure(model, "failure:breaker");
    const auto first_propagation = ltfa::propagate_failure(
        model, snapshot, ltfa::FailureInjection::for_declared_mode(breaker));
    const auto second_propagation = ltfa::propagate_failure(
        model, snapshot, ltfa::FailureInjection::for_declared_mode(breaker));
    expect(
        event_facts(model, first_propagation) == event_facts(model, second_propagation) &&
            first_propagation.usage.scheduled_events == second_propagation.usage.scheduled_events &&
            first_propagation.usage.processed_events == second_propagation.usage.processed_events &&
            first_propagation.usage.state_transitions == second_propagation.usage.state_transitions,
        "identical propagation must produce stable ordered facts and usage");

    const auto repeated_model = compile_fixture();
    ltfa::ModelOutputContext context;
    context.model_inputs = {"main.ltfa"};
    expect(ltfa::render_model_json(model, context) ==
                   ltfa::render_model_json(repeated_model, context) &&
               ltfa::render_model_dot(model, context) ==
                   ltfa::render_model_dot(repeated_model, context),
           "normalized Slice 2 JSON and DOT must be byte-stable");

    ltfa::SpofBudget candidate_budget;
    candidate_budget.max_candidates = 1;
    const auto spof_limited = ltfa::analyze_spof(model, snapshot, request, candidate_budget);
    expect(spof_limited.status == ltfa::AnalysisStatus::incomplete &&
               spof_limited.usage.candidates_considered == 1,
           "SPOF candidate budget exhaustion must be explicit");

    ltfa::PropagationBudget event_budget;
    event_budget.max_events = 1;
    const auto propagation_limited = ltfa::propagate_failure(
        model, snapshot, ltfa::FailureInjection::for_declared_mode(breaker), event_budget);
    expect(propagation_limited.status == ltfa::PropagationStatus::incomplete &&
               propagation_limited.usage.scheduled_events == 1,
           "propagation event budget exhaustion must be explicit");

    ltfa::TraversalBudget traversal_budget;
    traversal_budget.max_visited_objects = 1;
    const auto path_limited = ltfa::find_path(
        model, snapshot, request.source, request.targets.front(), request.view, traversal_budget);
    expect(path_limited.status == ltfa::AnalysisStatus::incomplete,
           "path work budget exhaustion must be explicit");
}

} // namespace

int main() {
    const auto model = compile_fixture();
    test_verified_redundancy_and_common_cause(model);
    test_breaker_and_fixed_delay(model);
    test_partial_model_qualification(model);
    test_determinism_and_budgets(model);
    return 0;
}
