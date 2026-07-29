#include "ltfa/dsl/parser.hpp"
#include "ltfa/model_compiler.hpp"
#include "ltfa/path_analysis.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

constexpr ltfa::StringId kFile{7};

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

ltfa::NormalizedModel compile_fixture() {
    const std::string source =
        "ltfa 1\n"
        "scope site:a kind=site completeness=verified evidence-quality=confirmed\n"
        "scope site:b kind=site completeness=partial evidence-quality=inferred\n"
        "entity service:source kind=service plane=service layer=application scope=site:a\n"
        "entity service:a kind=service plane=service layer=application scope=site:a "
        "failure-domain=shared-a\n"
        "entity service:b kind=service plane=service layer=application scope=site:a "
        "failure-domain=shared-a\n"
        "entity service:target kind=service plane=service layer=application scope=site:a\n"
        "entity service:partial kind=service plane=service layer=application scope=site:b\n"
        "entity overlay:vpn kind=tunnel plane=communication layer=overlay scope=site:a\n"
        "entity device:router kind=device plane=communication layer=physical scope=site:a\n"
        "entity device:switch kind=device plane=communication layer=physical scope=site:a\n"
        "entity facility:panel kind=facility plane=facility layer=physical scope=site:a\n"
        "endpoint endpoint:router:net owner=device:router kind=network\n"
        "endpoint endpoint:switch:net owner=device:switch kind=network\n"
        "endpoint endpoint:panel:out owner=facility:panel kind=power-output\n"
        "endpoint endpoint:router:power owner=device:router kind=power-input\n"
        "relate service:source -> service:a kind=depends_on\n"
        "relate service:source -> service:b kind=depends_on\n"
        "relate service:a -> service:b kind=depends_on\n"
        "relate service:a -> service:target kind=depends_on\n"
        "relate service:b -> service:target kind=depends_on\n"
        "relate service:target -> service:partial kind=depends_on\n"
        "relate overlay:vpn -> device:router kind=uses_underlay\n"
        "relate device:router -> device:switch kind=depends_on\n"
        "connect endpoint:router:net -- endpoint:switch:net kind=network\n"
        "connect endpoint:panel:out -> endpoint:router:power kind=power\n";

    auto parsed = ltfa::dsl::parse_document(source, kFile);
    expect(parsed.ok(), "path fixture must parse");
    ltfa::dsl::LoadedDocument loaded;
    loaded.statements = std::move(parsed.document->statements);
    loaded.total_source_bytes = source.size();
    auto compiled = ltfa::compile_model(loaded);
    expect(compiled.ok(), "path fixture must compile");
    return std::move(*compiled.model);
}

ltfa::ObjectRef find_object(const ltfa::NormalizedModel &model, std::string_view external_id) {
    for (const auto &entity : model.entities) {
        if (model.string(entity.external_id) == external_id) {
            return ltfa::ObjectRef::from(entity.id);
        }
    }
    for (const auto &endpoint : model.endpoints) {
        if (model.string(endpoint.external_id) == external_id) {
            return ltfa::ObjectRef::from(endpoint.id);
        }
    }
    return {};
}

ltfa::RelationId find_relation(const ltfa::NormalizedModel &model, std::string_view from,
                               std::string_view to, ltfa::RelationKind kind) {
    for (const auto &relation : model.relations) {
        if (model.external_id(relation.from) == from && model.external_id(relation.to) == to &&
            relation.kind == kind) {
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

ltfa::ScopeId find_scope(const ltfa::NormalizedModel &model, std::string_view external_id) {
    for (const auto &scope : model.scopes) {
        if (model.string(scope.external_id) == external_id) {
            return scope.id;
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

void test_reachability_and_path(const ltfa::NormalizedModel &model) {
    const auto source = find_object(model, "service:source");
    const auto service_a = find_object(model, "service:a");
    const auto service_b = find_object(model, "service:b");
    const auto target = find_object(model, "service:target");
    const auto partial = find_object(model, "service:partial");
    auto snapshot = ltfa::make_availability_snapshot(model);
    expect(snapshot.set(partial, ltfa::AvailabilityState::unknown),
           "partial object state must be writable");

    const auto reachability = ltfa::analyze_reachability(model, snapshot, source);
    expect(reachability.status == ltfa::AnalysisStatus::complete &&
               object_ids(model, reachability.objects) ==
                   std::vector<std::string_view>{"service:a", "service:b", "service:partial",
                                                 "service:source", "service:target"},
           "forward reachability must be complete and stable");
    expect(reachability.qualification.completeness == ltfa::Completeness::partial &&
               reachability.qualification.evidence_quality == ltfa::EvidenceQuality::inferred &&
               reachability.contains_unknown_availability,
           "reachability must retain partial, inferred, and unknown qualification");
    const auto repeated_reachability = ltfa::analyze_reachability(model, snapshot, source);
    expect(repeated_reachability.objects == reachability.objects &&
               repeated_reachability.usage.visited_objects == reachability.usage.visited_objects &&
               repeated_reachability.usage.examined_relations ==
                   reachability.usage.examined_relations,
           "repeated reachability must have deterministic objects and usage");

    const auto path = ltfa::find_path(model, snapshot, source, target);
    expect(path.status == ltfa::AnalysisStatus::complete && path.reachable &&
               object_ids(model, path.objects) ==
                   std::vector<std::string_view>{"service:source", "service:a", "service:target"} &&
               path.relations.size() == 2,
           "path selection must choose the deterministic shortest path");
    expect(path.qualification.completeness == ltfa::Completeness::verified &&
               !path.contains_unknown_availability,
           "path qualification must cover only path objects");
    const auto repeated_path = ltfa::find_path(model, snapshot, source, target);
    expect(repeated_path.objects == path.objects && repeated_path.relations == path.relations &&
               repeated_path.usage.visited_objects == path.usage.visited_objects &&
               repeated_path.usage.examined_relations == path.usage.examined_relations,
           "representative path and usage must be deterministic");

    ltfa::GraphFilter reverse;
    reverse.direction = ltfa::TraversalDirection::reverse;
    const auto reverse_reachability = ltfa::analyze_reachability(model, snapshot, target, reverse);
    expect(object_ids(model, reverse_reachability.objects) ==
               std::vector<std::string_view>{"service:a", "service:b", "service:source",
                                             "service:target"},
           "reverse traversal must follow incoming directed relations");

    ltfa::GraphFilter undirected;
    undirected.direction = ltfa::TraversalDirection::undirected;
    const auto undirected_reachability =
        ltfa::analyze_reachability(model, snapshot, target, undirected);
    expect(object_ids(model, undirected_reachability.objects) ==
               std::vector<std::string_view>{"service:a", "service:b", "service:partial",
                                             "service:source", "service:target"},
           "undirected traversal must cover both sides of directed relations");

    ltfa::GraphFilter no_unknown;
    no_unknown.availability.unknown = false;
    const auto known = ltfa::analyze_reachability(model, snapshot, source, no_unknown);
    expect(object_ids(model, known.objects) ==
                   std::vector<std::string_view>{"service:a", "service:b", "service:source",
                                                 "service:target"} &&
               known.qualification.completeness == ltfa::Completeness::verified &&
               !known.contains_unknown_availability,
           "availability selection must exclude unknown objects exactly");

    expect(snapshot.set(service_a, ltfa::AvailabilityState::unavailable) &&
               snapshot.set(service_b, ltfa::AvailabilityState::unavailable),
           "parallel providers must be writable");
    ltfa::GraphFilter operational;
    operational.availability.unavailable = false;
    const auto unavailable_path = ltfa::find_path(model, snapshot, source, target, operational);
    expect(unavailable_path.status == ltfa::AnalysisStatus::complete && !unavailable_path.reachable,
           "availability-filtered path must not traverse unavailable objects");
}

void test_filters_and_directions(const ltfa::NormalizedModel &model) {
    const auto overlay = find_object(model, "overlay:vpn");
    const auto router = find_object(model, "device:router");
    const auto network_source = find_object(model, "endpoint:router:net");
    const auto network_target = find_object(model, "endpoint:switch:net");
    const auto power_source = find_object(model, "endpoint:panel:out");
    const auto power_target = find_object(model, "endpoint:router:power");
    const auto service_source = find_object(model, "service:source");
    const auto service_a = find_object(model, "service:a");
    const auto service_target = find_object(model, "service:target");
    const auto shared = find_domain(model, "shared-a");
    const auto site_a = find_scope(model, "site:a");
    const auto site_b = find_scope(model, "site:b");
    const auto snapshot = ltfa::make_availability_snapshot(model);

    const auto cross_layer = ltfa::find_path(model, snapshot, overlay, router);
    expect(cross_layer.reachable, "unfiltered path must cross layer boundaries");

    ltfa::GraphFilter communication;
    communication.plane = ltfa::Plane::communication;
    expect(ltfa::find_path(model, snapshot, overlay, router, communication).reachable,
           "plane filter must retain objects in the selected plane");

    ltfa::GraphFilter overlay_only = communication;
    overlay_only.layer = ltfa::Layer::overlay;
    expect(!ltfa::find_path(model, snapshot, overlay, router, overlay_only).reachable,
           "layer filter must exclude a target in another layer");

    ltfa::GraphFilter site_a_only;
    site_a_only.scope = site_a;
    expect(ltfa::find_path(model, snapshot, service_source, service_target, site_a_only).reachable,
           "scope filter must retain a path wholly inside the selected scope");
    site_a_only.scope = site_b;
    expect(!ltfa::find_path(model, snapshot, service_source, service_target, site_a_only).reachable,
           "scope filter must reject a source outside the selected scope");

    ltfa::GraphFilter network_only;
    network_only.relation_kinds = {ltfa::RelationKind::network};
    expect(ltfa::find_path(model, snapshot, network_source, network_target, network_only).reachable,
           "relation-kind filter must retain selected bidirectional connectivity");

    ltfa::GraphFilter power_forward;
    power_forward.relation_kinds = {ltfa::RelationKind::power};
    expect(
        ltfa::find_path(model, snapshot, power_source, power_target, power_forward).reachable &&
            !ltfa::find_path(model, snapshot, power_target, power_source, power_forward).reachable,
        "forward traversal must preserve directed power semantics");
    power_forward.direction = ltfa::TraversalDirection::reverse;
    expect(ltfa::find_path(model, snapshot, power_target, power_source, power_forward).reachable,
           "reverse traversal must explicitly invert directed reachability");

    ltfa::GraphFilter domain_only;
    domain_only.failure_domain = shared;
    const auto domain_reachability =
        ltfa::analyze_reachability(model, snapshot, service_a, domain_only);
    expect(object_ids(model, domain_reachability.objects) ==
               std::vector<std::string_view>{"service:a", "service:b"},
           "failure-domain filter must retain only selected members");
}

void test_removal_impact(const ltfa::NormalizedModel &model) {
    const auto source = find_object(model, "service:source");
    const auto service_a = find_object(model, "service:a");
    const auto target = find_object(model, "service:target");
    const auto partial = find_object(model, "service:partial");
    const auto shared = find_domain(model, "shared-a");
    const auto network_source = find_object(model, "endpoint:router:net");
    const auto network_target = find_object(model, "endpoint:switch:net");
    const auto source_to_b =
        find_relation(model, "service:source", "service:b", ltfa::RelationKind::depends_on);
    const auto target_to_partial =
        find_relation(model, "service:target", "service:partial", ltfa::RelationKind::depends_on);
    auto snapshot = ltfa::make_availability_snapshot(model);
    expect(snapshot.set(partial, ltfa::AvailabilityState::unknown),
           "partial object state must be writable");

    const auto object_impact = ltfa::analyze_removal_impact(
        model, snapshot, source, ltfa::RemovalTarget::for_object(service_a));
    expect(object_impact.status == ltfa::AnalysisStatus::complete &&
               object_ids(model, object_impact.affected_objects) ==
                   std::vector<std::string_view>{"service:a"},
           "removing one redundant branch must not affect the target");

    const auto redundant_relation = ltfa::analyze_removal_impact(
        model, snapshot, source, ltfa::RemovalTarget::for_relation(source_to_b));
    expect(redundant_relation.status == ltfa::AnalysisStatus::complete &&
               redundant_relation.affected_objects.empty(),
           "removing one parallel relation must retain reachability");

    const auto downstream_relation = ltfa::analyze_removal_impact(
        model, snapshot, source, ltfa::RemovalTarget::for_relation(target_to_partial));
    expect(object_ids(model, downstream_relation.affected_objects) ==
               std::vector<std::string_view>{"service:partial"},
           "removing one downstream relation must identify its lost object");

    const auto target_impact = ltfa::analyze_removal_impact(
        model, snapshot, source, ltfa::RemovalTarget::for_object(target));
    expect(object_ids(model, target_impact.affected_objects) ==
               std::vector<std::string_view>{"service:partial", "service:target"},
           "removing an object must include it and its downstream loss");

    ltfa::GraphFilter network_only;
    network_only.relation_kinds = {ltfa::RelationKind::network};
    const auto endpoint_impact =
        ltfa::analyze_removal_impact(model, snapshot, network_source,
                                     ltfa::RemovalTarget::for_object(network_target), network_only);
    expect(object_ids(model, endpoint_impact.removed_objects) ==
                   std::vector<std::string_view>{"endpoint:switch:net"} &&
               endpoint_impact.affected_objects == endpoint_impact.removed_objects,
           "endpoint removal must report the removed endpoint as lost reachability");

    const auto domain_impact = ltfa::analyze_removal_impact(
        model, snapshot, source, ltfa::RemovalTarget::for_failure_domain(shared));
    expect(object_ids(model, domain_impact.removed_objects) ==
                   std::vector<std::string_view>{"service:a", "service:b"} &&
               object_ids(model, domain_impact.affected_objects) ==
                   std::vector<std::string_view>{"service:a", "service:b", "service:partial",
                                                 "service:target"} &&
               domain_impact.qualification.completeness == ltfa::Completeness::partial &&
               domain_impact.contains_unknown_availability,
           "domain removal must identify common-cause downstream impact and qualification");
}

void test_budget_and_invalid_input(const ltfa::NormalizedModel &model) {
    const auto source = find_object(model, "service:source");
    const auto target = find_object(model, "service:target");
    const auto snapshot = ltfa::make_availability_snapshot(model);

    ltfa::TraversalBudget small;
    small.max_visited_objects = 2;
    const auto incomplete = ltfa::analyze_reachability(model, snapshot, source, {}, small);
    expect(incomplete.status == ltfa::AnalysisStatus::incomplete &&
               incomplete.usage.visited_objects == 2,
           "visited-object budget must stop with an incomplete partial result");

    ltfa::TraversalBudget no_edges;
    no_edges.max_examined_relations = 0;
    const auto incomplete_path = ltfa::find_path(model, snapshot, source, target, {}, no_edges);
    expect(incomplete_path.status == ltfa::AnalysisStatus::incomplete && !incomplete_path.reachable,
           "relation budget exhaustion must not claim a target is unreachable");

    const auto invalid_source = ltfa::analyze_reachability(model, snapshot, ltfa::ObjectRef{});
    expect(invalid_source.status == ltfa::AnalysisStatus::invalid,
           "invalid source must return invalid status");

    ltfa::GraphFilter invalid_scope;
    invalid_scope.scope = ltfa::ScopeId{99};
    expect(ltfa::analyze_reachability(model, snapshot, source, invalid_scope).status ==
               ltfa::AnalysisStatus::invalid,
           "invalid filter IDs must return invalid status");

    ltfa::AvailabilitySnapshot undersized;
    expect(ltfa::analyze_reachability(model, undersized, source).status ==
               ltfa::AnalysisStatus::invalid,
           "undersized availability snapshots must return invalid status");

    const auto invalid_removal = ltfa::analyze_removal_impact(
        model, snapshot, source, ltfa::RemovalTarget::for_relation(ltfa::RelationId{99}));
    expect(invalid_removal.status == ltfa::AnalysisStatus::invalid,
           "invalid removal targets must return invalid status");
}

} // namespace

int main() {
    const auto model = compile_fixture();
    test_reachability_and_path(model);
    test_filters_and_directions(model);
    test_removal_impact(model);
    test_budget_and_invalid_input(model);
    return 0;
}
