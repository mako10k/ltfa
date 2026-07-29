# M2 Requirement And Failure-Domain Semantics Evidence

Date: 2026-07-29

Task: `M2_REQUIREMENT_SEMANTICS`

Status: verified

## Implemented Boundary

This task turns the normalized requirement-group and failure-domain records
accepted in Slice 1 into a dependency-free availability-semantics core.

Delivered in `include/ltfa/availability.hpp`:

- model-sized entity and endpoint availability snapshots with conservative
  `unknown` reads for omitted state
- exact `all` and `any` evaluation over `available`, `degraded`, `unavailable`,
  and `unknown`
- deterministic evaluation of every named group for one consumer, with
  multiple groups combined using `all`
- fixed `hold_up_ms` retained on each group evaluation for the later scheduled
  propagation task
- consumer results qualified by their scope's completeness and evidence quality
- failure-domain expansion ordered by stable external ID
- conservative completeness and evidence aggregation across domain members
- transactional application of one domain failure to all current entity and
  endpoint members in a model-sized snapshot

Empty provider sets evaluate to `unknown`; invalid model IDs and malformed
normalized ranges return no result instead of inventing availability.

## Explicit Deferrals

This task does not:

- schedule hold-up expiration or run a fixed-point propagation queue
- interpret failure modes or produce immediate/delayed affected-function
  findings
- run path, impact, articulation, bridge, or SPOF analysis
- add an assurance CLI command or change the `v0.1.0` CLI schemas

Those boundaries remain with `M2_FAILURE_PROPAGATION`, `M2_PATH_IMPACT`,
`M2_SPOF_ANALYSIS`, and `M2_ACCEPTANCE`.

## Requirement Trace

| Requirement | Evidence |
| --- | --- |
| `REQ-MOD-011` | Exhaustive pairwise `all`/`any` tests and multiple-group combination tests cover the specified truth tables. |
| `REQ-MOD-012` | Tests expand and apply overlapping many-to-many domains to entity and endpoint members. |
| `REQ-MOD-013` | Group evaluations retain normalized fixed hold-up milliseconds without prematurely scheduling them. |
| `REQ-MOD-014` | Every four-state combination is evaluated; missing state remains conservative `unknown`. |
| `REQ-MOD-015` | Consumer and failure-domain results retain or conservatively aggregate completeness and evidence quality. |
| `REQ-MOD-016..018` | The existing desired-model separation, inert extensions, and versioned failure-mode catalog remain unchanged. |

## Verification

The following committed configurations were rebuilt and tested from the same
source state:

```text
Release:            17/17
ASan/UBSan:         15/15
LeakSanitizer:      15/15
ASan/UBSan + seeds: 16/16
```

`ltfa.assurance.requirements` covers:

- all 16 ordered state pairs for both `all` and `any`
- empty, single-provider, missing-state, and invalid-ID behavior
- multiple required groups and fixed hold-up retention
- consumer qualification from partial/inferred scope metadata
- stable domain ordering, overlapping membership, entity and endpoint failure
  application, and mutation-free rejection of invalid or undersized input

The Release suite also retained the static and runtime offline-boundary checks,
complete CLI `strace`, install-tree smoke, and external Graphviz validation.
The implementation adds no file, network, process, environment, database, or
plugin access.
