#pragma once

#include "ltfa/availability.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace ltfa {

enum class AnalysisStatus {
    complete,
    incomplete,
    invalid,
};

enum class TraversalDirection {
    forward,
    reverse,
    undirected,
};

struct AvailabilitySelection {
    bool available{true};
    bool degraded{true};
    bool unavailable{true};
    bool unknown{true};

    [[nodiscard]] bool includes(AvailabilityState state) const noexcept;
};

struct GraphFilter {
    std::optional<Plane> plane;
    std::optional<Layer> layer;
    std::optional<ScopeId> scope;
    std::optional<FailureDomainId> failure_domain;
    std::vector<RelationKind> relation_kinds;
    AvailabilitySelection availability;
    TraversalDirection direction{TraversalDirection::forward};
};

struct TraversalBudget {
    std::size_t max_visited_objects{100'000};
    std::size_t max_examined_relations{500'000};
};

struct TraversalUsage {
    std::size_t visited_objects{0};
    std::size_t examined_relations{0};
};

struct ReachabilityResult {
    AnalysisStatus status{AnalysisStatus::invalid};
    ObjectRef source;
    std::vector<ObjectRef> objects;
    AssuranceQualification qualification;
    bool contains_unknown_availability{false};
    TraversalUsage usage;
};

struct PathResult {
    AnalysisStatus status{AnalysisStatus::invalid};
    ObjectRef source;
    ObjectRef target;
    bool reachable{false};
    std::vector<ObjectRef> objects;
    std::vector<RelationId> relations;
    AssuranceQualification qualification;
    bool contains_unknown_availability{false};
    TraversalUsage usage;
};

enum class RemovalKind {
    object,
    relation,
    failure_domain,
};

struct RemovalTarget {
    RemovalKind kind{RemovalKind::object};
    ObjectRef object_value;
    RelationId relation_value;
    FailureDomainId failure_domain_value;

    [[nodiscard]] static constexpr RemovalTarget for_object(ObjectRef object) noexcept {
        RemovalTarget value;
        value.kind = RemovalKind::object;
        value.object_value = object;
        return value;
    }

    [[nodiscard]] static constexpr RemovalTarget for_relation(RelationId relation) noexcept {
        RemovalTarget value;
        value.kind = RemovalKind::relation;
        value.relation_value = relation;
        return value;
    }

    [[nodiscard]] static constexpr RemovalTarget
    for_failure_domain(FailureDomainId domain) noexcept {
        RemovalTarget value;
        value.kind = RemovalKind::failure_domain;
        value.failure_domain_value = domain;
        return value;
    }
};

struct ImpactResult {
    AnalysisStatus status{AnalysisStatus::invalid};
    ObjectRef source;
    RemovalTarget removal;
    std::vector<ObjectRef> removed_objects;
    std::vector<RelationId> removed_relations;
    std::vector<ObjectRef> affected_objects;
    AssuranceQualification qualification;
    bool contains_unknown_availability{false};
    TraversalUsage usage;
};

[[nodiscard]] ReachabilityResult analyze_reachability(const NormalizedModel &model,
                                                      const AvailabilitySnapshot &snapshot,
                                                      ObjectRef source,
                                                      const GraphFilter &filter = {},
                                                      const TraversalBudget &budget = {});

[[nodiscard]] PathResult find_path(const NormalizedModel &model,
                                   const AvailabilitySnapshot &snapshot, ObjectRef source,
                                   ObjectRef target, const GraphFilter &filter = {},
                                   const TraversalBudget &budget = {});

[[nodiscard]] ImpactResult analyze_removal_impact(const NormalizedModel &model,
                                                  const AvailabilitySnapshot &snapshot,
                                                  ObjectRef source, RemovalTarget removal,
                                                  const GraphFilter &filter = {},
                                                  const TraversalBudget &budget = {});

} // namespace ltfa
