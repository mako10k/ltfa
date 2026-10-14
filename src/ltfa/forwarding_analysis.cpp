// Responsibility: validate a declared ring and bound post-switch reachability.
#include "ltfa/forwarding_analysis.hpp"
#include <algorithm>
#include <map>
#include <queue>
#include <set>
namespace ltfa {
std::string_view to_string(ForwardingState s) noexcept {
    switch (s) {
    case ForwardingState::forwarding:
        return "forwarding";
    case ForwardingState::blocking:
        return "blocking";
    case ForwardingState::unavailable:
        return "unavailable";
    case ForwardingState::unknown:
        return "unknown";
    }
    return "unknown";
}
std::string_view to_string(ForwardingReachability s) noexcept {
    switch (s) {
    case ForwardingReachability::reachable:
        return "reachable";
    case ForwardingReachability::unreachable:
        return "unreachable";
    case ForwardingReachability::unknown:
        return "unknown";
    }
    return "unknown";
}
namespace {
std::string_view attr(const NormalizedModel &m, IndexRange r, std::string_view key) {
    if (r.begin > m.attributes.size() || r.count > m.attributes.size() - r.begin)
        return {};
    for (std::size_t i = r.begin; i < r.begin + r.count; ++i)
        if (m.string(m.attributes[i].key) == key)
            return m.string(m.attributes[i].value);
    return {};
}
AvailabilityState combine(std::initializer_list<AvailabilityState> states) {
    bool unknown = false, degraded = false;
    for (auto s : states) {
        if (s == AvailabilityState::unavailable)
            return s;
        unknown |= s == AvailabilityState::unknown;
        degraded |= s == AvailabilityState::degraded;
    }
    return unknown    ? AvailabilityState::unknown
           : degraded ? AvailabilityState::degraded
                      : AvailabilityState::available;
}
} // namespace
ForwardingResult analyze_forwarding(const NormalizedModel &m, const AvailabilitySnapshot &s,
                                    EntityId domain, ObjectRef source, const TraversalBudget &b) {
    ForwardingResult r;
    r.domain = domain;
    r.source = source;
    auto reject = [&](std::string reason, AnalysisStatus status = AnalysisStatus::invalid) {
        r.status = status;
        r.reasons.push_back(std::move(reason));
        return r;
    };
    auto examine = [&]() {
        if (r.usage.examined_relations >= b.max_examined_relations)
            return false;
        ++r.usage.examined_relations;
        return true;
    };
    auto visit = [&]() {
        if (r.usage.visited_objects >= b.max_visited_objects)
            return false;
        ++r.usage.visited_objects;
        return true;
    };
    if (domain.value() >= m.entities.size() ||
        m.entities[domain.value()].kind != EntityKind::control_domain)
        return reject("invalid-control-domain");
    if (s.entity_states.size() != m.entities.size() ||
        s.endpoint_states.size() != m.endpoints.size())
        return reject("invalid-physical-snapshot");
    auto valid_state = [](AvailabilityState state) {
        return state == AvailabilityState::available || state == AvailabilityState::degraded ||
               state == AvailabilityState::unavailable || state == AvailabilityState::unknown;
    };
    for (auto state : s.entity_states)
        if (!valid_state(state))
            return reject("invalid-physical-snapshot-state");
    for (auto state : s.endpoint_states)
        if (!valid_state(state))
            return reject("invalid-physical-snapshot-state");
    r.protocol = attr(m, m.entities[domain.value()].attributes, "protocol");
    if (r.protocol.empty())
        return reject("missing-protocol", AnalysisStatus::incomplete);
    if (r.protocol != "turbo-ring")
        return reject("unsupported-protocol", AnalysisStatus::incomplete);
    if (source.kind == ObjectKind::entity && source.index < m.entities.size())
        r.source_device = EntityId{source.index};
    else if (source.kind == ObjectKind::endpoint && source.index < m.endpoints.size() &&
             m.endpoints[source.index].kind == EndpointKind::network)
        r.source_device = m.endpoints[source.index].owner;
    else
        return reject("invalid-source");
    std::set<EntityId> devices, links;
    std::map<EntityId, std::string> policy;
    for (const auto &rel : m.relations) {
        if (!examine())
            return reject("work-budget-exhausted", AnalysisStatus::incomplete);
        if (rel.kind != RelationKind::member_of || rel.to != ObjectRef::from(domain))
            continue;
        if (rel.from.kind != ObjectKind::entity || rel.from.index >= m.entities.size())
            return reject("invalid-member");
        EntityId id{rel.from.index};
        auto kind = m.entities[id.value()].kind;
        if (kind == EntityKind::device) {
            if (!devices.insert(id).second)
                return reject("duplicate-member");
        } else if (kind == EntityKind::link) {
            if (!links.insert(id).second)
                return reject("duplicate-member");
            policy[id] = attr(m, rel.attributes, "normal-state");
        } else
            return reject("unsupported-member-kind");
    }
    if (devices.size() < 3 || links.size() != devices.size())
        return reject("not-simple-cycle");
    if (!devices.count(r.source_device))
        return reject("source-outside-domain");
    auto order = [&](EntityId a, EntityId c) {
        return m.external_id(ObjectRef::from(a)) < m.external_id(ObjectRef::from(c));
    };
    std::vector<EntityId> sortedlinks(links.begin(), links.end()),
        sorteddevices(devices.begin(), devices.end());
    std::sort(sortedlinks.begin(), sortedlinks.end(), order);
    std::sort(sorteddevices.begin(), sorteddevices.end(), order);
    std::map<EntityId, std::vector<EndpointId>> ports;
    for (const auto &rel : m.relations) {
        if (!examine())
            return reject("work-budget-exhausted", AnalysisStatus::incomplete);
        if (rel.kind != RelationKind::network)
            continue;
        for (auto pair : {std::make_pair(rel.from, rel.to), std::make_pair(rel.to, rel.from)})
            if (pair.first.kind == ObjectKind::entity && links.count(EntityId{pair.first.index})) {
                if (pair.second.kind != ObjectKind::endpoint ||
                    pair.second.index >= m.endpoints.size() ||
                    m.endpoints[pair.second.index].kind != EndpointKind::network ||
                    rel.direction != Direction::bidirectional)
                    return reject("invalid-link-endpoint");
                ports[EntityId{pair.first.index}].push_back(EndpointId{pair.second.index});
            }
    }
    std::set<EndpointId> used_ports;
    std::map<EntityId, std::vector<std::size_t>> adjacency;
    std::set<std::pair<EntityId, EntityId>> pairs;
    std::size_t backups = 0;
    bool failure = false, unknown = false;
    for (auto id : sortedlinks) {
        if (!visit())
            return reject("work-budget-exhausted", AnalysisStatus::incomplete);
        auto pp = ports[id];
        if (pp.size() != 2 || pp[0] == pp[1])
            return reject("link-requires-two-network-endpoints");
        if (!used_ports.insert(pp[0]).second || !used_ports.insert(pp[1]).second)
            return reject("reused-ring-endpoint");
        auto a = m.endpoints[pp[0].value()].owner, c = m.endpoints[pp[1].value()].owner;
        if (!devices.count(a) || !devices.count(c) || a == c)
            return reject("foreign-or-self-link");
        if (!pairs.insert(std::minmax(a, c)).second)
            return reject("parallel-links");
        if (order(c, a)) {
            std::swap(a, c);
            std::swap(pp[0], pp[1]);
        }
        auto p = policy[id];
        bool backup = p == "blocking" || p == "standby" || p == "discarding";
        if (!backup && p != "forwarding")
            return reject("missing-or-unsupported-normal-state", AnalysisStatus::incomplete);
        backups += backup;
        auto physical = combine({s.state(ObjectRef::from(id)), s.state(ObjectRef::from(a)),
                                 s.state(ObjectRef::from(c)), s.state(ObjectRef::from(pp[0])),
                                 s.state(ObjectRef::from(pp[1]))});
        failure |= physical == AvailabilityState::unavailable;
        unknown |= physical == AvailabilityState::unknown;
        r.contains_degraded_availability |= physical == AvailabilityState::degraded;
        std::size_t i = r.links.size();
        adjacency[a].push_back(i);
        adjacency[c].push_back(i);
        r.links.push_back({id, pp[0], pp[1], a, c, physical, ForwardingState::unknown, backup,
                           s.state(ObjectRef::from(id))});
    }
    if (backups != 1)
        return reject("requires-exactly-one-backup", AnalysisStatus::incomplete);
    for (auto id : devices)
        if (adjacency[id].size() != 2)
            return reject("not-simple-cycle");
    std::set<EntityId> connected;
    std::queue<EntityId> q;
    q.push(sorteddevices[0]);
    connected.insert(sorteddevices[0]);
    while (!q.empty()) {
        auto id = q.front();
        q.pop();
        if (!visit())
            return reject("work-budget-exhausted", AnalysisStatus::incomplete);
        for (auto i : adjacency[id]) {
            if (!examine())
                return reject("work-budget-exhausted", AnalysisStatus::incomplete);
            auto &l = r.links[i];
            auto other = l.first_device == id ? l.second_device : l.first_device;
            if (connected.insert(other).second)
                q.push(other);
        }
    }
    if (connected.size() != devices.size())
        return reject("disconnected-cycle");
    auto ds = s.state(ObjectRef::from(domain));
    unknown |= ds == AvailabilityState::unknown;
    r.contains_degraded_availability |= ds == AvailabilityState::degraded;
    r.contains_unknown_availability = unknown;
    r.status = unknown ? AnalysisStatus::incomplete : AnalysisStatus::complete;
    r.qualification = {Completeness::verified, EvidenceQuality::confirmed};
    auto qualify = [&](ScopeId scope) {
        if (scope.value() >= m.scopes.size()) {
            r.qualification = {Completeness::unknown, EvidenceQuality::unknown};
            return;
        }
        const auto &sc = m.scopes[scope.value()];
        if (static_cast<int>(sc.completeness) > static_cast<int>(r.qualification.completeness))
            r.qualification.completeness = sc.completeness;
        if (static_cast<int>(sc.evidence_quality) >
            static_cast<int>(r.qualification.evidence_quality))
            r.qualification.evidence_quality = sc.evidence_quality;
    };
    qualify(m.entities[domain.value()].scope);
    for (auto id : sorteddevices)
        qualify(m.entities[id.value()].scope);
    for (const auto &l : r.links) {
        qualify(m.entities[l.link.value()].scope);
        qualify(m.endpoints[l.first_port.value()].scope);
        qualify(m.endpoints[l.second_port.value()].scope);
    }
    if (source.kind == ObjectKind::endpoint) {
        qualify(m.endpoints[source.index].scope);
        auto st = s.state(source);
        unknown |= st == AvailabilityState::unknown;
        r.contains_unknown_availability = unknown;
        if (st == AvailabilityState::unknown)
            r.status = AnalysisStatus::incomplete;
        r.contains_degraded_availability |= st == AvailabilityState::degraded;
    }
    if (unknown) {
        if (r.qualification.completeness == Completeness::verified)
            r.qualification.completeness = Completeness::partial;
        r.reasons.push_back("unknown-physical-availability");
    }
    if (r.contains_degraded_availability)
        r.reasons.push_back("degraded-physical-availability");
    for (auto &l : r.links) {
        if (l.physical == AvailabilityState::unavailable || ds == AvailabilityState::unavailable)
            l.logical = ForwardingState::unavailable;
        else if (ds == AvailabilityState::unknown || l.physical == AvailabilityState::unknown ||
                 (l.backup && !failure && unknown))
            l.logical = ForwardingState::unknown;
        else if (l.backup && !failure)
            l.logical = ForwardingState::blocking;
        else
            l.logical = ForwardingState::forwarding;
        if (l.backup) {
            r.backup_activated = failure && l.logical == ForwardingState::forwarding;
            r.backup_activation_unknown = l.logical == ForwardingState::unknown;
        }
    }
    // Lower and upper graphs bound every unknown physical state and backup activation.
    std::map<EntityId, std::pair<EntityId, EntityId>> predecessor;
    auto traverse = [&](bool possible, std::set<EntityId> &seen) {
        auto ss = combine({s.state(ObjectRef::from(r.source_device)), s.state(source)});
        if (ds == AvailabilityState::unavailable || ss == AvailabilityState::unavailable)
            return true;
        if (!possible && (ds == AvailabilityState::unknown || ss == AvailabilityState::unknown))
            return true;
        std::queue<EntityId> pending;
        pending.push(r.source_device);
        seen.insert(r.source_device);
        while (!pending.empty()) {
            auto id = pending.front();
            pending.pop();
            if (!visit())
                return false;
            for (auto i : adjacency[id]) {
                if (!examine())
                    return false;
                const auto &l = r.links[i];
                if (l.logical == ForwardingState::unavailable ||
                    l.logical == ForwardingState::blocking ||
                    (!possible && l.logical == ForwardingState::unknown))
                    continue;
                auto other = l.first_device == id ? l.second_device : l.first_device;
                if (seen.insert(other).second) {
                    pending.push(other);
                    if (!possible)
                        predecessor[other] = {id, l.link};
                }
            }
        }
        return true;
    };
    std::set<EntityId> lower, upper;
    bool exhausted = !traverse(false, lower);
    if (!exhausted)
        exhausted = !traverse(true, upper);
    if (exhausted) {
        r.status = AnalysisStatus::incomplete;
        if (r.qualification.completeness == Completeness::verified)
            r.qualification.completeness = Completeness::partial;
        r.reasons.push_back("work-budget-exhausted");
    }
    for (auto id : sorteddevices) {
        ForwardingTargetResult t;
        t.target = id;
        t.reachability = lower.count(id)                ? ForwardingReachability::reachable
                         : exhausted || upper.count(id) ? ForwardingReachability::unknown
                                                        : ForwardingReachability::unreachable;
        if (t.reachability == ForwardingReachability::reachable) {
            auto cur = id;
            t.devices.push_back(cur);
            while (cur != r.source_device) {
                auto p = predecessor.at(cur);
                t.links.push_back(p.second);
                cur = p.first;
                t.devices.push_back(cur);
            }
            std::reverse(t.devices.begin(), t.devices.end());
            std::reverse(t.links.begin(), t.links.end());
        }
        r.targets.push_back(std::move(t));
    }
    return r;
}
} // namespace ltfa
