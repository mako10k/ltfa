// Responsibility: validate Turbo Ring switching, uncertainty and topology rejection.
#include "ltfa/dsl/parser.hpp"
#include "ltfa/forwarding_analysis.hpp"
#include "ltfa/model_compiler.hpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>
using namespace ltfa;
void check(bool ok, const char *message) {
    if (!ok) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}
NormalizedModel fixture() {
    std::string text = "ltfa 1\nscope site:r kind=site completeness=verified "
                       "evidence-quality=confirmed\nentity ring:r kind=control-domain "
                       "plane=communication layer=l2 scope=site:r protocol=turbo-ring\n";
    for (int i = 0; i < 5; ++i) {
        auto n = std::to_string(i);
        text += "entity sw:" + n +
                " kind=device plane=communication layer=physical scope=site:r\nendpoint port:" + n +
                ":a owner=sw:" + n + " kind=network\nendpoint port:" + n + ":b owner=sw:" + n +
                " kind=network\nrelate sw:" + n + " -> ring:r kind=member_of\n";
    }
    for (int i = 0; i < 5; ++i) {
        auto n = std::to_string(i), j = std::to_string((i + 1) % 5);
        text += "entity link:" + n +
                " kind=link plane=communication layer=physical scope=site:r\nrelate link:" + n +
                " -> ring:r kind=member_of normal-state=" + (i == 4 ? "blocking" : "forwarding") +
                "\nconnect port:" + n + ":a -- link:" + n + " kind=network\nconnect link:" + n +
                " -- port:" + j + ":b kind=network\n";
    }
    auto p = dsl::parse_document(text, StringId{0});
    check(p.ok(), "parse");
    dsl::LoadedDocument d;
    d.statements = std::move(p.document->statements);
    auto c = compile_model(d);
    if (!c.ok()) {
        std::cerr << c.error->message << "\n";
    }
    check(c.ok(), "compile");
    return std::move(*c.model);
}
ObjectRef object(const NormalizedModel &m, std::string_view name) {
    for (auto &e : m.entities)
        if (m.string(e.external_id) == name)
            return ObjectRef::from(e.id);
    for (auto &e : m.endpoints)
        if (m.string(e.external_id) == name)
            return ObjectRef::from(e.id);
    std::exit(2);
}
std::size_t count(const ForwardingResult &r, ForwardingReachability state) {
    return std::count_if(r.targets.begin(), r.targets.end(),
                         [&](auto &t) { return t.reachability == state; });
}
int main() {
    auto m = fixture();
    EntityId domain{object(m, "ring:r").index};
    auto source = object(m, "sw:0");
    auto healthy = make_availability_snapshot(m);
    auto a = analyze_forwarding(m, healthy, domain, source);
    check(a.status == AnalysisStatus::complete &&
              count(a, ForwardingReachability::reachable) == 5 && !a.backup_activated,
          "normal ring");
    check(a.links.back().logical == ForwardingState::blocking &&
              a.links.back().physical == AvailabilityState::available,
          "block separate physical");
    for (int i = 0; i < 5; ++i) {
        auto s = healthy;
        (void)s.set(object(m, "link:" + std::to_string(i)), AvailabilityState::unavailable);
        auto r = analyze_forwarding(m, s, domain, source);
        check(count(r, ForwardingReachability::reachable) == 5, "single link fault connectivity");
        check(r.backup_activated == (i != 4), "backup activation");
        check(healthy.state(object(m, "link:" + std::to_string(i))) == AvailabilityState::available,
              "immutable snapshot");
    }
    for (auto name : {"sw:2", "port:2:a"}) {
        auto s = healthy;
        (void)s.set(object(m, name), AvailabilityState::unavailable);
        auto r = analyze_forwarding(m, s, domain, source);
        check(r.backup_activated && count(r, ForwardingReachability::reachable) ==
                                        (std::string(name) == "sw:2" ? 4 : 5),
              "device/port fault");
    }
    auto s = healthy;
    (void)s.set(object(m, "link:0"), AvailabilityState::unavailable);
    (void)s.set(object(m, "link:2"), AvailabilityState::unavailable);
    check(count(analyze_forwarding(m, s, domain, source), ForwardingReachability::unreachable) == 2,
          "two fault partition");
    s = healthy;
    (void)s.set(object(m, "link:0"), AvailabilityState::unknown);
    auto u = analyze_forwarding(m, s, domain, source);
    check(u.status == AnalysisStatus::incomplete && u.backup_activation_unknown &&
              count(u, ForwardingReachability::unknown) == 4,
          "unknown link bounds");
    s = healthy;
    (void)s.set(object(m, "sw:2"), AvailabilityState::unknown);
    check(analyze_forwarding(m, s, domain, source).status == AnalysisStatus::incomplete,
          "unknown device");
    s = healthy;
    (void)s.set(ObjectRef::from(domain), AvailabilityState::unknown);
    check(count(analyze_forwarding(m, s, domain, source), ForwardingReachability::unknown) == 5,
          "unknown domain");
    (void)s.set(ObjectRef::from(domain), AvailabilityState::unavailable);
    check(count(analyze_forwarding(m, s, domain, source), ForwardingReachability::unreachable) == 5,
          "unavailable domain");
    s = healthy;
    (void)s.set(object(m, "link:0"), AvailabilityState::degraded);
    u = analyze_forwarding(m, s, domain, source);
    check(u.status == AnalysisStatus::complete && u.contains_degraded_availability &&
              count(u, ForwardingReachability::reachable) == 5,
          "degraded traversable");
    s = healthy;
    (void)s.set(source, AvailabilityState::unavailable);
    check(count(analyze_forwarding(m, s, domain, source), ForwardingReachability::unreachable) == 5,
          "source unavailable");
    check(analyze_forwarding(m, healthy, domain, object(m, "port:0:a")).source_device ==
              EntityId{source.index},
          "source endpoint owner");
    s = healthy;
    s.entity_states.clear();
    check(analyze_forwarding(m, s, domain, source).status == AnalysisStatus::invalid,
          "invalid snapshot");
    check(analyze_forwarding(m, healthy, domain, source, {0, 1}).status ==
              AnalysisStatus::incomplete,
          "budget topology");
    u = analyze_forwarding(m, healthy, domain, source, {17, 1000});
    check(u.status == AnalysisStatus::incomplete, "budget traversal cumulative");
    check(count(analyze_forwarding(m, healthy, domain, source),
                ForwardingReachability::reachable) == 5,
          "repair fresh snapshot");
    auto reverse = m;
    std::reverse(reverse.relations.begin(), reverse.relations.end());
    auto z = analyze_forwarding(reverse, healthy, domain, source);
    check(z.links.size() == a.links.size() && z.targets.size() == a.targets.size(),
          "order dimensions");
    for (std::size_t i = 0; i < a.targets.size(); ++i)
        check(a.targets[i].links == z.targets[i].links, "deterministic paths");
    auto broken = m;
    broken.relations.push_back(broken.relations[0]);
    check(analyze_forwarding(broken, healthy, domain, source).status == AnalysisStatus::invalid,
          "duplicate membership");

    s = healthy;
    (void)s.set(object(m, "port:0:a"), AvailabilityState::unavailable);
    check(count(analyze_forwarding(m, s, domain, object(m, "port:0:a")),
                ForwardingReachability::unreachable) == 5,
          "source port unavailable");
    s = healthy;
    (void)s.set(object(m, "port:0:a"), AvailabilityState::unknown);
    check(count(analyze_forwarding(m, s, domain, object(m, "port:0:a")),
                ForwardingReachability::unknown) == 5,
          "source port unknown");
    s = healthy;
    (void)s.set(source, AvailabilityState::unknown);
    check(count(analyze_forwarding(m, s, domain, source), ForwardingReachability::unknown) == 5,
          "source unknown");
    auto protocol = m;
    for (auto &v : protocol.attributes)
        if (protocol.string(v.key) == "protocol") {
            protocol.strings[v.value.value()] = "rstp";
        }
    check(analyze_forwarding(protocol, healthy, domain, source).reasons[0] ==
              "unsupported-protocol",
          "distinct unsupported protocol");
    auto no_backup = m;
    for (auto &v : no_backup.attributes)
        if (no_backup.string(v.key) == "normal-state" && no_backup.string(v.value) == "blocking")
            no_backup.strings[v.value.value()] = "forwarding";
    check(analyze_forwarding(no_backup, healthy, domain, source).status ==
              AnalysisStatus::incomplete,
          "missing backup");
    auto two_backup = m;
    for (auto &v : two_backup.attributes)
        if (two_backup.string(v.key) == "normal-state" &&
            two_backup.string(v.value) == "forwarding") {
            two_backup.strings[v.value.value()] = "blocking";
            break;
        }
    check(analyze_forwarding(two_backup, healthy, domain, source).status ==
              AnalysisStatus::incomplete,
          "ambiguous backup");
    auto quality = m;
    quality.scopes.push_back(quality.scopes[0]);
    quality.scopes.back().id = ScopeId{1};
    quality.scopes.back().completeness = Completeness::unknown;
    quality.scopes.back().evidence_quality = EvidenceQuality::unknown;
    quality.entities[object(m, "link:0").index].scope = ScopeId{1};
    auto qr = analyze_forwarding(quality, healthy, domain, source);
    check(qr.qualification.completeness == Completeness::unknown &&
              qr.qualification.evidence_quality == EvidenceQuality::unknown,
          "link scope worst qualification");
    auto reused = m;
    for (auto &rel : reused.relations) {
        if (rel.kind != RelationKind::network)
            continue;
        if (rel.to == object(m, "link:0")) {
            rel.from = object(m, "port:4:b");
            break;
        }
        if (rel.from == object(m, "link:0")) {
            rel.to = object(m, "port:4:b");
            break;
        }
    }
    check(analyze_forwarding(reused, healthy, domain, source).status == AnalysisStatus::invalid,
          "reused physical endpoint");
    auto missing = m;
    for (auto it = missing.relations.begin(); it != missing.relations.end(); ++it)
        if (it->kind == RelationKind::network) {
            missing.relations.erase(it);
            break;
        }
    check(analyze_forwarding(missing, healthy, domain, source).status == AnalysisStatus::invalid,
          "missing network attachment");
    auto directed = m;
    for (auto &rel : directed.relations)
        if (rel.kind == RelationKind::network) {
            rel.direction = Direction::directed;
            break;
        }
    check(analyze_forwarding(directed, healthy, domain, source).status == AnalysisStatus::invalid,
          "directed physical attachment rejected");
    auto extra = m;
    for (const auto &rel : m.relations)
        if (rel.kind == RelationKind::network) {
            extra.relations.push_back(rel);
            break;
        }
    check(analyze_forwarding(extra, healthy, domain, source).status == AnalysisStatus::invalid,
          "extra physical attachment rejected");
    s = healthy;
    s.entity_states[0] = static_cast<AvailabilityState>(999);
    check(analyze_forwarding(m, s, domain, source).status == AnalysisStatus::invalid,
          "invalid state enum rejected");
    std::cout << "Turbo Ring forwarding tests passed\n";
}
