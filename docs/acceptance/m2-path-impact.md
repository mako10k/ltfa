# M2 Filtered Path And Removal-Impact Evidence

Date: 2026-07-29

Task: `M2_PATH_IMPACT`

Status: verified

## Implemented Boundary

This task adds dependency-free graph analysis over the normalized adjacency
indexes accepted in Slice 1. It uses those indexes directly and does not copy
the full graph into a second representation.

Delivered in `include/ltfa/path_analysis.hpp`:

- bounded breadth-first reachability from an explicit selected object
- deterministic representative shortest paths for unweighted graph views
- forward, reverse, and undirected traversal over typed relations
- plane, layer, scope, failure-domain, relation-kind, and simulated
  availability filters
- entity and endpoint removal impact
- relation removal impact
- many-to-many failure-domain removal impact
- stable external-ID ordering for reachability, removed objects, and affected
  objects
- `complete`, `incomplete`, and `invalid` outcomes with visited-object and
  examined-relation usage
- completeness, evidence-quality, and unknown-availability qualification of
  returned objects

The explicit source selects the service or other object whose reachable view
is analyzed. Representative path tie-breaking follows normalized relation-kind
order and the stable adjacency order produced by the model compiler. Removal
impact is the set difference between baseline reachability and reachability
after the selected removal. Each baseline and changed traversal is separately
bounded by `TraversalBudget`.

## Explicit Deferrals

This task does not:

- assign edge weights or invent a cost model
- enumerate node or edge cuts
- classify articulation points, bridges, or SPOFs
- propagate typed availability requirements or schedule hold-up expiration
- interpret declared failure modes or produce a failure timeline
- add an assurance CLI command or change the `v0.1.0` CLI schemas

Those boundaries remain with `M2_SPOF_ANALYSIS`, `M2_FAILURE_PROPAGATION`, and
`M2_ACCEPTANCE`.

## Requirement Trace

| Requirement | Evidence |
| --- | --- |
| `REQ-ANL-001` | Tests cover same-layer paths, an overlay-to-physical cross-layer path, forward/reverse/undirected direction, and every declared graph-view filter. |
| `REQ-ANL-002` | Tests remove an entity, endpoint, directed relation, redundant relation, and shared failure domain and compare lost baseline reachability. |
| Basic design 10.1 | The analyzer traverses normalized adjacency in place, uses unweighted BFS, accepts simulated availability, and returns stable representative paths. |
| Basic design 10.5 | Entity, endpoint, relation, and failure-domain injection targets are represented without prematurely implementing propagation or failure timelines. |

## Verification

The following committed configurations were rebuilt and tested from the same
source state:

```text
Release:            18/18
ASan/UBSan:         16/16
LeakSanitizer:      16/16
ASan/UBSan + seeds: 17/17
```

`ltfa.analysis.path-impact` covers:

- deterministic repeated reachability, path, and resource-usage results
- same-layer and cross-layer paths
- plane, layer, scope, relation-kind, failure-domain, and four-state
  availability views
- forward, reverse, and undirected traversal
- redundant and downstream entity or relation removal
- endpoint removal and shared-domain common-cause impact
- result qualification from partial, inferred, and unknown objects
- visited-object and examined-relation budget exhaustion
- invalid object, relation, scope, and availability-snapshot input

The Release suite also retained static and runtime offline-boundary checks,
complete CLI `strace`, install-tree smoke, and external Graphviz validation.
The implementation adds no file, network, process, environment, database, or
plugin access.
