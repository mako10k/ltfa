# M2 Offline Assurance Slice Acceptance

Date: 2026-07-29

Task: `M2_ACCEPTANCE`

Status: accepted

## Accepted Boundary

Slice 2 is accepted as a dependency-free, one-shot C++ core API for offline
assurance analysis. It combines:

- requirement-group availability with `all`, `any`, and fixed hold-up
- many-to-many failure-domain expansion
- bounded filtered reachability and representative paths
- modeled object, relation, and failure-domain removal impact
- articulation-point and bridge analysis
- qualified object, relation, and common-domain SPOF findings
- deterministic single-failure and fixed-delay propagation

The acceptance fixture in `tests/fixtures/slice2/main.ltfa` is one reviewable
model containing 16 entities, 11 relations, three requirement groups, three
failure domains, one declared breaker mode, verified and partial scopes,
independent redundancy, and modeled shared causes.

The fixture is synthetic. It contains no credentials, secret references, live
addresses, customer identifiers, observation input, or production
authorization.

## Cross-Feature Acceptance

`ltfa.acceptance.slice2` loads and normalizes that fixture through the real
model-root-safe include path, then verifies:

- one link in a redundant path retains a representative source-to-target path
- the two redundant path members share one route failure domain and simultaneous
  removal stops the selected target
- an articulation point in the verified view is `confirmed-spof`
- the same structural risks in a partial and inferred view are only
  `model-spof`
- a surviving path in the partial view remains
  `not-a-spof-in-view` with partial qualification
- a breaker trip loses the breaker and immediate service at time zero
- the held service and its dependent are lost at exactly 30 seconds
- independent backup power retains the redundant service after one breaker
  failure
- shared power-domain failure removes both providers and loses that service
- repeated SPOF and propagation calls retain ordered facts and exact usage
- two independent compilations render byte-identical model JSON and DOT
- path, SPOF-candidate, and propagation-event budgets return explicit
  `incomplete` results

## Review Gates

| Gate | Result |
| --- | --- |
| Redundant link versus shared route | Pass: one link retains a path; the shared domain affects the selected target. |
| Breaker immediate and delayed effects | Pass: exact events occur at 0 ms and 30,000 ms. |
| Partial-model claims | Pass: no partial or inferred affected view produces `confirmed-spof`. |
| Deterministic ordering | Pass: repeated facts, usage, JSON, and DOT are stable. |
| Resource limits | Pass: all three integrated analyzer families report bounded incompleteness. |
| Offline execution | Pass: the integrated executable produces no network syscall under `strace`. |
| Secret independence | Pass: the synthetic fixture needs no secret or environment input. |
| Scope control | Pass: no observation decoder, diagnosis, cut enumeration, live probe, or CLI expansion is introduced. |

## Requirement Trace

| Requirement | Evidence |
| --- | --- |
| `AC-003` | The verified diamond retains a path after one relation removal, while its shared route domain produces a common-cause SPOF. |
| `AC-004` | The declared breaker mode produces exact immediate and 30-second delayed service losses; the shared power domain also removes the backup provider. |
| `AC-007` | Identical models and budgets produce stable ordered analysis facts and usage; independent normalization produces byte-identical JSON and DOT. |
| `AC-008` | Static core checks and a dedicated `strace` acceptance test reject network behavior; the fixture needs no secret. |
| Basic design Slice 2 | Every listed Slice 2 feature is implemented and exercised without adding Slice 3 or Slice 4 behavior. |

## Verification

The same source state passed:

```text
Release:            22/22
ASan/UBSan:         19/19
LeakSanitizer:      19/19
ASan/UBSan + seeds: 20/20
```

Release includes the dedicated
`ltfa.boundary.slice2-offline-runtime` network-syscall check in addition to the
static archive boundary, CLI runtime boundary, install-tree smoke, and external
Graphviz render validation.

## Release And Deferred Work

Reaching `SLICE2_ACCEPTED` establishes the implementation boundary identified
for a later `v0.2.0`; it does not change the current `0.1.0` binary version,
create a release candidate or tag, or authorize publication.

Observation JSON Lines, freshness and conflicting evidence, diagnosis,
service-oriented cuts, and analysis CLI exposure remain outside this accepted
slice.
