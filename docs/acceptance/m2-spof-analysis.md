# M2 Qualified SPOF Analysis Evidence

Date: 2026-07-29

Task: `M2_SPOF_ANALYSIS`

Status: verified

## Implemented Boundary

This task adds bounded structural and source-to-target analysis over an
explicit undirected view of the normalized model.

Delivered in `include/ltfa/spof_analysis.hpp`:

- iterative Tarjan articulation-point and bridge analysis in `O(V + E)`
- plane, layer, scope, failure-domain, relation-kind, and availability views
- distinct relation identities so parallel realizations do not become false
  bridges
- explicit source and target context on structural results
- automatic articulation, bridge, and shared failure-domain candidates
- explicit object, relation, and failure-domain candidates
- baseline and retained representative path evidence
- affected object and target sets
- shared-domain and common-cause member evidence
- completeness, evidence-quality, unknown-availability, and
  filter-exclusion qualification
- `confirmed-spof`, `model-spof`, and `not-a-spof-in-view`
- candidate and cumulative graph-work budgets with `complete`, `incomplete`,
  and `invalid` results

Structural analysis always resolves the requested view to undirected traversal.
The source and target sets qualify the result but do not limit the Tarjan scan
to one connected component.

Automatic SPOF analysis first requires every selected target to be reachable
from the selected source. It then tests non-terminal articulation points,
bridges, and failure domains containing at least two selected members. Explicit
candidates also retain negative findings when a path survives.

## Classification Policy

`confirmed-spof` requires all of the following:

- removing the candidate disconnects at least one selected target
- every selected object's scope is `verified`
- evidence quality is `confirmed`
- no selected object has unknown availability
- the graph filter did not exclude another modeled realization

If removal disconnects a target but any qualification is weaker, the result is
`model-spof`. If the selected registered view retains a target path, the result
is `not-a-spof-in-view`; it is never emitted as a global sufficiency or safety
claim.

Unknown objects relevant to baseline paths, retained paths, affected objects,
or common-cause members are listed in `remaining_unknowns`. A relation-kind or
object-view filter that excludes another modeled edge is retained as
`alternatives_excluded_by_filter`.

## Explicit Deferrals

This task does not:

- enumerate node or edge cut sets of size two
- assign edge weights, probability, severity, or occurrence
- infer failures or reconcile observation conflicts
- limit the structural scan to a service causal cone
- add an assurance CLI command or change the accepted `v0.1.0` CLI schemas

Cut enumeration remains with `M3_CUTS`; cross-feature acceptance remains with
`M2_ACCEPTANCE`.

## Requirement Trace

| Requirement | Evidence |
| --- | --- |
| `REQ-ANL-003` | The focused fixture identifies exactly one articulation point and one bridge while preserving a redundant diamond. |
| `REQ-ANL-006` | Two individually redundant path members in one failure domain become one common-cause SPOF candidate with both members identified. |
| `REQ-ANL-013` | Findings carry affected objects and targets, baseline or retained path evidence, common domains and members, qualification, filtered-alternative state, and relevant remaining unknowns. |
| `REQ-ANL-014` | Partial or inferred scopes, unknown availability, and excluded alternatives produce only `model-spof`; surviving paths produce the explicitly view-limited negative classification. |
| `AC-003` | Removing one redundant relation or object retains a path, while removing the shared domain loses the selected target. |
| Basic design 10.2 | Iterative Tarjan analysis uses relation identity in an undirected selected view and returns deterministic articulation and bridge sets. |
| Basic design 10.6 | Removal outcomes use the three specified qualified classifications. |

## Verification

The following committed configurations were rebuilt and tested from the same
source state:

```text
Release:            20/20
ASan/UBSan:         18/18
LeakSanitizer:      18/18
ASan/UBSan + seeds: 19/19
```

`ltfa.analysis.spof` covers:

- exact articulation-point and bridge results
- an individually redundant object and relation
- a shared-domain common-cause failure
- deterministic repeated findings, paths, ordering, and usage
- verified, partial, inferred, unknown-availability, and filtered-alternative
  qualification
- candidate, visited-object, and examined-relation budget exhaustion
- malformed relation identity and view enum, undersized snapshots, and invalid
  terminal candidates
- stable public classification labels

The Release suite also retained static and runtime offline-boundary checks,
complete CLI `strace`, install-tree smoke, and external Graphviz validation.
The implementation adds no file, network, process, environment, database, or
plugin access.
