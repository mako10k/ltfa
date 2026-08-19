#pragma once

#include "ltfa/path_analysis.hpp"

#include <cstddef>
#include <vector>

namespace ltfa {

enum class CutElementKind {
    object,
    relation,
    failure_domain,
};

struct CutElement {
    CutElementKind kind{CutElementKind::object};
    ObjectRef object_value;
    RelationId relation_value;
    FailureDomainId failure_domain_value;

    [[nodiscard]] static constexpr CutElement for_object(ObjectRef object) noexcept {
        CutElement value;
        value.kind = CutElementKind::object;
        value.object_value = object;
        return value;
    }

    [[nodiscard]] static constexpr CutElement for_relation(RelationId relation) noexcept {
        CutElement value;
        value.kind = CutElementKind::relation;
        value.relation_value = relation;
        return value;
    }

    [[nodiscard]] static constexpr CutElement for_failure_domain(FailureDomainId domain) noexcept {
        CutElement value;
        value.kind = CutElementKind::failure_domain;
        value.failure_domain_value = domain;
        return value;
    }
};

struct ServiceCutRequest {
    ObjectRef source;
    ObjectRef target;
    GraphFilter view;
    std::vector<CutElement> candidates;
    std::size_t max_cut_size{2};
};

struct ServiceCutBudget {
    std::size_t max_candidate_sets{10'000};
    std::size_t max_results{1'024};
    std::size_t max_total_visited_objects{2'000'000};
    std::size_t max_total_examined_relations{10'000'000};
    std::size_t max_flow_transitions{10'000'000};
};

struct ServiceCutUsage {
    std::size_t candidate_sets_considered{0};
    std::size_t visited_objects{0};
    std::size_t examined_relations{0};
    std::size_t flow_transitions{0};
};

struct ServiceCutFinding {
    std::vector<CutElement> elements;
    std::vector<ObjectRef> removed_objects;
    std::vector<RelationId> removed_relations;
    std::vector<FailureDomainId> common_domains;
    AssuranceQualification qualification;
    bool contains_unknown_availability{false};
};

struct ServiceCutAnalysisResult {
    AnalysisStatus status{AnalysisStatus::invalid};
    ServiceCutRequest request;
    bool baseline_reachable{false};
    std::size_t minimum_object_cut{0};
    std::size_t minimum_relation_cut{0};
    std::vector<ServiceCutFinding> findings;
    ServiceCutUsage usage;
};

[[nodiscard]] ServiceCutAnalysisResult analyze_service_cuts(const NormalizedModel &model,
                                                            const AvailabilitySnapshot &snapshot,
                                                            const ServiceCutRequest &request,
                                                            const ServiceCutBudget &budget = {});

} // namespace ltfa
