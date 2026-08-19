#include "ltfa/cut_analysis.hpp"
#include "ltfa/dsl/parser.hpp"
#include "ltfa/model_compiler.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr ltfa::StringId kFile{17};

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

ltfa::NormalizedModel compile_fixture() {
    const std::string source =
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
        "relate service:source -> service:a kind=depends_on\n"
        "relate service:source -> service:b kind=depends_on\n"
        "relate service:a -> service:target kind=depends_on\n"
        "relate service:b -> service:target kind=depends_on\n";
    auto parsed = ltfa::dsl::parse_document(source, kFile);
    expect(parsed.ok(), "cut fixture must parse");
    ltfa::dsl::LoadedDocument loaded;
    loaded.statements = std::move(parsed.document->statements);
    loaded.total_source_bytes = source.size();
    auto compiled = ltfa::compile_model(loaded);
    expect(compiled.ok(), "cut fixture must compile");
    return std::move(*compiled.model);
}

ltfa::ObjectRef find_object(const ltfa::NormalizedModel &model, std::string_view id) {
    for (const auto &entity : model.entities) {
        if (model.string(entity.external_id) == id) {
            return ltfa::ObjectRef::from(entity.id);
        }
    }
    return {};
}

bool has_finding(const ltfa::ServiceCutAnalysisResult &result, ltfa::CutElementKind kind,
                 std::size_t size) {
    for (const auto &finding : result.findings) {
        if (finding.elements.size() == size &&
            std::all_of(finding.elements.begin(), finding.elements.end(),
                        [&](const ltfa::CutElement &element) { return element.kind == kind; })) {
            return true;
        }
    }
    return false;
}

void test_bounded_cuts(const ltfa::NormalizedModel &model) {
    ltfa::ServiceCutRequest request;
    request.source = find_object(model, "service:source");
    request.target = find_object(model, "service:target");
    const auto snapshot = ltfa::make_availability_snapshot(model);
    const auto result = ltfa::analyze_service_cuts(model, snapshot, request);
    expect(result.status == ltfa::AnalysisStatus::complete && result.baseline_reachable,
           "diamond baseline must be reachable and complete");
    expect(result.minimum_object_cut == 2 && result.minimum_relation_cut == 2,
           "node-split and relation max-flow must both report a minimum cut of two");
    expect(has_finding(result, ltfa::CutElementKind::object, 2),
           "two individually redundant objects must form a service cut");
    expect(has_finding(result, ltfa::CutElementKind::relation, 2),
           "two individually redundant relations must form a service cut");
    expect(has_finding(result, ltfa::CutElementKind::failure_domain, 1),
           "shared failure domain must be emitted as a common-cause scenario");
    expect(result.usage.candidate_sets_considered > 0 && result.usage.flow_transitions > 0,
           "bounded analysis must expose enumeration and flow work usage");

    const auto repeated = ltfa::analyze_service_cuts(model, snapshot, request);
    expect(repeated.status == result.status &&
               repeated.minimum_object_cut == result.minimum_object_cut &&
               repeated.minimum_relation_cut == result.minimum_relation_cut &&
               repeated.usage.candidate_sets_considered == result.usage.candidate_sets_considered &&
               repeated.usage.flow_transitions == result.usage.flow_transitions &&
               repeated.findings.size() == result.findings.size(),
           "repeated service-cut analysis must be deterministic");
}

void test_incomplete_and_invalid(const ltfa::NormalizedModel &model) {
    ltfa::ServiceCutRequest request;
    request.source = find_object(model, "service:source");
    request.target = find_object(model, "service:target");
    const auto snapshot = ltfa::make_availability_snapshot(model);

    ltfa::ServiceCutBudget no_flow;
    no_flow.max_flow_transitions = 0;
    expect(ltfa::analyze_service_cuts(model, snapshot, request, no_flow).status ==
               ltfa::AnalysisStatus::incomplete,
           "flow budget exhaustion must return a structured incomplete result");

    ltfa::ServiceCutBudget no_sets;
    no_sets.max_candidate_sets = 0;
    expect(ltfa::analyze_service_cuts(model, snapshot, request, no_sets).status ==
               ltfa::AnalysisStatus::incomplete,
           "candidate-set budget exhaustion must return incomplete");

    request.max_cut_size = 3;
    expect(ltfa::analyze_service_cuts(model, snapshot, request).status ==
               ltfa::AnalysisStatus::invalid,
           "cut sizes above the accepted bound must be invalid");
}

} // namespace

int main() {
    const auto model = compile_fixture();
    test_bounded_cuts(model);
    test_incomplete_and_invalid(model);
    return 0;
}
