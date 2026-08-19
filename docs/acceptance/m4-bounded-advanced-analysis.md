# Slice 4 bounded advanced-analysis acceptance

Date: 2026-08-19

## Accepted scope

Slice 4 integrates two source-tree C++17 core boundaries:

- bounded service cuts and common failure-domain scenarios;
- recommendation-only next-useful observations for tied diagnosis hypotheses.

The detailed contracts and focused evidence are recorded in `m4-bounded-cuts.md` and
`m4-next-observation.md`. `ltfa.acceptance.slice4` runs both accepted programs as one CTest gate.

## Acceptance matrix

| Concern | Evidence |
| --- | --- |
| object and relation cuts of size one and two | node-split/relation-capacity max-flow and exact deterministic findings |
| common-domain scenarios | failure-domain expansion finding with stable member order |
| work bounds | candidate, result, traversal, relation, flow, probe, and comparison exhaustion tests |
| incomplete results | exhausted budgets return structured `incomplete`, never `complete` |
| observation gaps | only registered probes without current evidence are considered |
| recommendation only | API returns probe IDs and predictions; it has no producer or execution interface |
| output stability | repeated inputs assert identical findings, recommendations, and usage counters |
| offline safety | dedicated strace network-syscall tests for both Slice 4 programs |
| regression safety | complete Release CTest suite |
| undefined behavior | focused Debug UBSan tests for both Slice 4 programs |

## Boundary

Slice 4 does not change CLI Contract 2 and does not install a core SDK. Live probes, adapters,
diagnosis CLI design, stable release work, benchmark claims, and publication remain separately
gated. The requirements and basic-design documents named by the residual PERT plan are not in this
checkout, so this acceptance does not claim textual traceability beyond the plan and retained
Slice 2/3 contracts.
