#include "ltfa/dsl/parser.hpp"
#include "ltfa/model_compiler.hpp"
#include "ltfa/spof_analysis.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr ltfa::StringId kFile{11};

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

ltfa::NormalizedModel compile_fixture(bool include_filtered_alternative = false,
                                      bool include_parallel_tail = false) {
    std::string source =
        "ltfa 1\n"
        "scope site:verified kind=site completeness=verified evidence-quality=confirmed\n"
        "entity service:source kind=service plane=service layer=application "
        "scope=site:verified\n"
        "entity service:a kind=service plane=service layer=application "
        "scope=site:verified failure-domain=shared-path\n"
        "entity service:b kind=service plane=service layer=application "
        "scope=site:verified failure-domain=shared-path\n"
        "entity service:target kind=service plane=service layer=application "
        "scope=site:verified\n"
        "entity service:tail kind=service plane=service layer=application "
        "scope=site:verified\n"
        "relate service:source -> service:a kind=depends_on\n"
        "relate service:source -> service:b kind=depends_on\n"
        "relate service:a -> service:target kind=depends_on\n"
        "relate service:b -> service:target kind=depends_on\n"
        "relate service:target -> service:tail kind=depends_on\n";
    if (include_filtered_alternative) {
        source += "relate service:source -> service:tail kind=routes_via\n";
    }
    if (include_parallel_tail) {
        source += "relate service:target -> service:tail kind=routes_via\n";
    }

    auto parsed = ltfa::dsl::parse_document(source, kFile);
    expect(parsed.ok(), "SPOF fixture must parse");
    ltfa::dsl::LoadedDocument loaded;
    loaded.statements = std::move(parsed.document->statements);
    loaded.total_source_bytes = source.size();
    auto compiled = ltfa::compile_model(loaded);
    expect(compiled.ok(), "SPOF fixture must compile");
    return std::move(*compiled.model);
}

ltfa::ObjectRef find_object(const ltfa::NormalizedModel &model, std::string_view external_id) {
    for (const auto &entity : model.entities) {
        if (model.string(entity.external_id) == external_id) {
            return ltfa::ObjectRef::from(entity.id);
        }
    }
    return {};
}

ltfa::RelationId find_relation(const ltfa::NormalizedModel &model, std::string_view left,
                               std::string_view right, ltfa::RelationKind kind) {
    for (const auto &relation : model.relations) {
        const auto from = model.external_id(relation.from);
        const auto to = model.external_id(relation.to);
        if (relation.kind == kind &&
            ((from == left && to == right) || (from == right && to == left))) {
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

std::vector<std::string_view> object_ids(const ltfa::NormalizedModel &model,
                                         const std::vector<ltfa::ObjectRef> &objects) {
    std::vector<std::string_view> result;
    for (const auto object : objects) {
        result.push_back(model.external_id(object));
    }
    return result;
}

const ltfa::SpofFinding *find_finding(const ltfa::SpofAnalysisResult &result,
                                      ltfa::RemovalKind kind) {
    for (const auto &finding : result.findings) {
        if (finding.candidate.kind == kind) {
            return &finding;
        }
    }
    return nullptr;
}

void expect_same_finding(const ltfa::SpofFinding &left, const ltfa::SpofFinding &right) {
    expect(left.candidate.kind == right.candidate.kind &&
               left.candidate.object_value == right.candidate.object_value &&
               left.candidate.relation_value == right.candidate.relation_value &&
               left.candidate.failure_domain_value == right.candidate.failure_domain_value &&
               left.classification == right.classification &&
               left.affected_targets == right.affected_targets &&
               left.affected_objects == right.affected_objects &&
               left.common_cause_members == right.common_cause_members &&
               left.common_domains == right.common_domains &&
               left.remaining_unknowns == right.remaining_unknowns,
           "repeated SPOF findings must be deterministic");
}

void test_structural_and_confirmed_spof(const ltfa::NormalizedModel &model) {
    const auto source = find_object(model, "service:source");
    const auto target = find_object(model, "service:target");
    const auto tail = find_object(model, "service:tail");
    const auto bridge =
        find_relation(model, "service:target", "service:tail", ltfa::RelationKind::depends_on);
    const auto shared_path = find_domain(model, "shared-path");
    const auto snapshot = ltfa::make_availability_snapshot(model);

    const auto structural =
        ltfa::analyze_articulation_bridges(model, snapshot, {}, {source}, {tail});
    expect(structural.status == ltfa::AnalysisStatus::complete &&
               object_ids(model, structural.articulation_points) ==
                   std::vector<std::string_view>{"service:target"} &&
               structural.bridges == std::vector<ltfa::RelationId>{bridge},
           "Tarjan analysis must identify the articulation and bridge exactly");
    expect(structural.view.direction == ltfa::TraversalDirection::undirected &&
               structural.qualification.completeness == ltfa::Completeness::verified &&
               structural.qualification.evidence_quality == ltfa::EvidenceQuality::confirmed &&
               !structural.contains_unknown_availability &&
               !structural.alternatives_excluded_by_filter,
           "structural results must retain their qualified undirected view");

    ltfa::SpofRequest request;
    request.source = source;
    request.targets = {tail};
    const auto result = ltfa::analyze_spof(model, snapshot, request);
    expect(result.status == ltfa::AnalysisStatus::complete && result.findings.size() == 3 &&
               result.usage.candidates_considered == 3,
           "automatic SPOF analysis must evaluate structural and common-cause candidates");

    const auto *object = find_finding(result, ltfa::RemovalKind::object);
    const auto *relation = find_finding(result, ltfa::RemovalKind::relation);
    const auto *domain = find_finding(result, ltfa::RemovalKind::failure_domain);
    expect(object != nullptr && object->candidate.object_value == target &&
               object->classification == ltfa::SpofClassification::confirmed_spof &&
               object_ids(model, object->affected_targets) ==
                   std::vector<std::string_view>{"service:tail"} &&
               object->baseline_paths.size() == 1,
           "articulation removal must be a confirmed SPOF with path evidence");
    expect(relation != nullptr && relation->candidate.relation_value == bridge &&
               relation->classification == ltfa::SpofClassification::confirmed_spof &&
               object_ids(model, relation->affected_targets) ==
                   std::vector<std::string_view>{"service:tail"},
           "bridge removal must be a confirmed SPOF");
    expect(domain != nullptr && domain->candidate.failure_domain_value == shared_path &&
               domain->classification == ltfa::SpofClassification::confirmed_spof &&
               object_ids(model, domain->common_cause_members) ==
                   std::vector<std::string_view>{"service:a", "service:b"} &&
               domain->common_domains == std::vector<ltfa::FailureDomainId>{shared_path},
           "shared failure-domain removal must expose modeled common-cause SPOF");

    const auto repeated = ltfa::analyze_spof(model, snapshot, request);
    expect(repeated.status == result.status && repeated.findings.size() == result.findings.size() &&
               repeated.usage.candidates_considered == result.usage.candidates_considered &&
               repeated.usage.visited_objects == result.usage.visited_objects &&
               repeated.usage.examined_relations == result.usage.examined_relations,
           "repeated SPOF analysis must have deterministic status and usage");
    for (std::size_t index = 0; index < result.findings.size(); ++index) {
        expect_same_finding(result.findings[index], repeated.findings[index]);
    }

    const auto parallel_model = compile_fixture(false, true);
    const auto parallel_snapshot = ltfa::make_availability_snapshot(parallel_model);
    const auto parallel = ltfa::analyze_articulation_bridges(parallel_model, parallel_snapshot);
    expect(parallel.status == ltfa::AnalysisStatus::complete && parallel.bridges.empty() &&
               object_ids(parallel_model, parallel.articulation_points) ==
                   std::vector<std::string_view>{"service:target"},
           "parallel relation identities must prevent a false bridge");
}

void test_redundancy_and_qualification(const ltfa::NormalizedModel &model) {
    const auto source = find_object(model, "service:source");
    const auto a = find_object(model, "service:a");
    const auto tail = find_object(model, "service:tail");
    const auto redundant_relation =
        find_relation(model, "service:source", "service:a", ltfa::RelationKind::depends_on);
    auto snapshot = ltfa::make_availability_snapshot(model);

    ltfa::SpofRequest request;
    request.source = source;
    request.targets = {tail};
    request.candidates = {ltfa::RemovalTarget::for_relation(redundant_relation)};
    const auto redundant = ltfa::analyze_spof(model, snapshot, request);
    expect(redundant.status == ltfa::AnalysisStatus::complete && redundant.findings.size() == 1 &&
               redundant.findings[0].classification ==
                   ltfa::SpofClassification::not_a_spof_in_view &&
               redundant.findings[0].affected_targets.empty() &&
               redundant.findings[0].retained_paths.size() == 1,
           "removing one redundant relation must retain an evidenced path");

    request.candidates = {ltfa::RemovalTarget::for_object(a)};
    const auto redundant_object = ltfa::analyze_spof(model, snapshot, request);
    expect(redundant_object.status == ltfa::AnalysisStatus::complete &&
               redundant_object.findings[0].classification ==
                   ltfa::SpofClassification::not_a_spof_in_view,
           "removing one redundant object must not be called a global safety claim");

    auto partial_model = model;
    partial_model.scopes.front().completeness = ltfa::Completeness::partial;
    partial_model.scopes.front().evidence_quality = ltfa::EvidenceQuality::inferred;
    const auto partial_snapshot = ltfa::make_availability_snapshot(partial_model);
    request.candidates.clear();
    const auto partial = ltfa::analyze_spof(partial_model, partial_snapshot, request);
    expect(partial.status == ltfa::AnalysisStatus::complete && !partial.findings.empty(),
           "partial-model SPOF analysis must remain usable");
    for (const auto &finding : partial.findings) {
        expect(finding.classification == ltfa::SpofClassification::model_spof &&
                   finding.qualification.completeness == ltfa::Completeness::partial &&
                   finding.qualification.evidence_quality == ltfa::EvidenceQuality::inferred,
               "partial or inferred evidence must qualify findings as model SPOFs");
    }

    expect(snapshot.set(a, ltfa::AvailabilityState::unknown),
           "SPOF fixture availability must be writable");
    const auto unknown = ltfa::analyze_spof(model, snapshot, request);
    expect(unknown.status == ltfa::AnalysisStatus::complete &&
               unknown.structural.contains_unknown_availability && !unknown.findings.empty(),
           "unknown availability must be preserved in the structural result");
    for (const auto &finding : unknown.findings) {
        expect(finding.classification == ltfa::SpofClassification::model_spof,
               "unknown availability must prevent a confirmed SPOF claim");
    }
    const auto *unknown_domain = find_finding(unknown, ltfa::RemovalKind::failure_domain);
    expect(unknown_domain != nullptr && object_ids(model, unknown_domain->remaining_unknowns) ==
                                            std::vector<std::string_view>{"service:a"},
           "SPOF evidence must identify relevant remaining unknown state");
}

void test_filter_exclusion_and_fail_closed() {
    const auto model = compile_fixture(true);
    const auto source = find_object(model, "service:source");
    const auto target = find_object(model, "service:target");
    const auto tail = find_object(model, "service:tail");
    auto snapshot = ltfa::make_availability_snapshot(model);

    ltfa::SpofRequest request;
    request.source = source;
    request.targets = {tail};
    request.view.relation_kinds = {ltfa::RelationKind::depends_on};
    const auto filtered = ltfa::analyze_spof(model, snapshot, request);
    expect(filtered.status == ltfa::AnalysisStatus::complete &&
               filtered.structural.alternatives_excluded_by_filter && !filtered.findings.empty(),
           "filtered alternatives must be explicit in the SPOF result");
    for (const auto &finding : filtered.findings) {
        expect(finding.classification == ltfa::SpofClassification::model_spof &&
                   finding.alternatives_excluded_by_filter,
               "excluded alternatives must prevent a confirmed SPOF claim");
    }

    request.candidates = {ltfa::RemovalTarget::for_object(target)};
    const auto filtered_explicit = ltfa::analyze_spof(model, snapshot, request);
    expect(filtered_explicit.findings.size() == 1 &&
               filtered_explicit.findings[0].classification == ltfa::SpofClassification::model_spof,
           "explicit filtered candidate must retain its qualification");

    ltfa::SpofBudget one_candidate;
    one_candidate.max_candidates = 1;
    request.candidates.clear();
    const auto limited = ltfa::analyze_spof(model, snapshot, request, one_candidate);
    expect(limited.status == ltfa::AnalysisStatus::incomplete && limited.findings.size() == 1 &&
               limited.usage.candidates_considered == 1,
           "candidate budget exhaustion must return an explicit partial result");

    ltfa::SpofBudget graph_work;
    graph_work.max_total_visited_objects = model.entities.size() * 2;
    graph_work.max_total_examined_relations = 100;
    request.candidates = {ltfa::RemovalTarget::for_object(target)};
    const auto work_limited = ltfa::analyze_spof(model, snapshot, request, graph_work);
    expect(work_limited.status == ltfa::AnalysisStatus::incomplete && work_limited.findings.empty(),
           "cumulative SPOF graph-work exhaustion must return incomplete");
    request.candidates.clear();

    ltfa::TraversalBudget structural_budget;
    structural_budget.max_visited_objects = 2;
    const auto structural =
        ltfa::analyze_articulation_bridges(model, snapshot, {}, {}, {}, structural_budget);
    expect(structural.status == ltfa::AnalysisStatus::incomplete,
           "structural work budget exhaustion must return incomplete");
    structural_budget.max_visited_objects = 100;
    structural_budget.max_examined_relations = 2;
    expect(
        ltfa::analyze_articulation_bridges(model, snapshot, {}, {}, {}, structural_budget).status ==
            ltfa::AnalysisStatus::incomplete,
        "structural relation budget exhaustion must return incomplete");

    snapshot.entity_states.pop_back();
    expect(ltfa::analyze_spof(model, snapshot, request).status == ltfa::AnalysisStatus::invalid,
           "snapshot/model size mismatch must fail closed");

    const auto valid_snapshot = ltfa::make_availability_snapshot(model);
    request.candidates = {ltfa::RemovalTarget::for_object(tail)};
    expect(ltfa::analyze_spof(model, valid_snapshot, request).status ==
               ltfa::AnalysisStatus::invalid,
           "source and target removal candidates must fail closed");

    auto malformed = model;
    malformed.relations.front().id = ltfa::RelationId{1};
    const auto malformed_snapshot = ltfa::make_availability_snapshot(malformed);
    expect(ltfa::analyze_articulation_bridges(malformed, malformed_snapshot).status ==
               ltfa::AnalysisStatus::invalid,
           "malformed normalized relation identity must fail closed");

    ltfa::GraphFilter malformed_filter;
    malformed_filter.direction = static_cast<ltfa::TraversalDirection>(99);
    expect(ltfa::analyze_articulation_bridges(model, valid_snapshot, malformed_filter).status ==
               ltfa::AnalysisStatus::invalid,
           "malformed view enums must fail closed");
}

void test_public_labels() {
    expect(ltfa::to_string(ltfa::SpofClassification::confirmed_spof) == "confirmed-spof" &&
               ltfa::to_string(ltfa::SpofClassification::model_spof) == "model-spof" &&
               ltfa::to_string(ltfa::SpofClassification::not_a_spof_in_view) ==
                   "not-a-spof-in-view",
           "public SPOF labels must be stable and qualified");
}

} // namespace

int main() {
    const auto model = compile_fixture();
    test_structural_and_confirmed_spof(model);
    test_redundancy_and_qualification(model);
    test_filter_exclusion_and_fail_closed();
    test_public_labels();
    return 0;
}
