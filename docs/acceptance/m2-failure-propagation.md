# M2 Deterministic Failure-Propagation Evidence

Date: 2026-07-29

Task: `M2_FAILURE_PROPAGATION`

Status: verified

## Implemented Boundary

This task adds one-shot failure propagation over the normalized acyclic
availability-dependency graph and the requirement semantics accepted earlier
in Slice 2.

Delivered in `include/ltfa/failure_propagation.hpp`:

- one entity or endpoint state injection
- one relation, failure-domain, or declared failure-mode injection
- direct typed dependencies combined as required inputs
- `all` and `any` requirement-group propagation
- fixed group hold-up that retains the previous effective state until expiry
- a time-and-stable-ID ordered event queue continued to a fixed point
- immediate unavailable, immediate degraded, delayed unavailable, delayed
  degraded, unaffected, and unknown classifications
- modeled detection gaps for affected objects without a registered direct probe
- registered probes lost when their target or vantage becomes unavailable
- completeness and evidence-quality qualification
- explicit event and state-transition budgets with `complete`, `incomplete`,
  and `invalid` results

The input snapshot is the caller's current fixed baseline. The analyzer changes
only the declared injection and its downstream effects; it does not reinterpret
unrelated pre-existing states as new scenario events.

A failure-domain injection changes every current domain member before any
member transition propagates. A failed normalized availability relation is
treated as an unavailable required input. Connectivity-only relations, such as
a network edge without an availability dependency, are retained as failed
relations but do not invent object-state propagation.

## Failure-Mode Policy

Built-in v1 modes have a fixed deterministic result:

- signal-quality or stale modes become `degraded`: `undervoltage`,
  `termination-fault`, `noise`, `logic-inverted`, `under-range`, `over-range`,
  `value-frozen`, and `heartbeat-stale`
- the remaining built-in hard-loss modes become `unavailable`
- an `x-` extension mode becomes `unknown` and sets
  `unresolved_failure_mode`; the core does not invent extension semantics

The declared `after` duration delays the initiating transition. Requirement
group `hold-up` begins when the desired group state differs from its effective
state. Multiple expirations at the same time use stable identifiers, and
timestamp addition fails closed on overflow.

## Explicit Deferrals

This task does not:

- inject multiple independent failures in one scenario
- model recovery, retries, flapping, battery curves, or analog process dynamics
- solve cyclic availability dependencies
- infer a failure mode from observations
- assign occurrence probability or conflate it with severity
- add an assurance CLI command or change the `v0.1.0` CLI schemas

Those boundaries remain with later diagnosis work and `M2_ACCEPTANCE`. SPOF
classification remains with `M2_SPOF_ANALYSIS`.

## Requirement Trace

| Requirement | Evidence |
| --- | --- |
| `REQ-ANL-007` | Tests propagate declared, object, endpoint, relation, and failure-domain injections through typed dependencies and `all`/`any` groups to a fixed point. |
| `REQ-ANL-008` | Results and tests retain immediate loss, degraded operation, delayed loss, unaffected objects, direct-probe detection gaps, lost probes, and unresolved `unknown` effects. |
| `REQ-ANL-009` | Two groups expiring at the same 30-second hold-up and a chained dependent produce stable delayed events; timestamp overflow fails closed. |
| `AC-004` | A modeled breaker causes immediate provider and consumer loss, 30-second delayed held-service loss, and a same-time downstream loss. |
| Basic design 10.4 | The implementation uses the acyclic normalized dependency adjacency, fixed group states, a min-heap event queue, and repeated consumer evaluation. |
| Basic design 10.5 | Every MVP injection target kind is represented while observation inference and multi-failure scenarios remain deferred. |

## Verification

The following committed configurations were rebuilt and tested from the same
source state:

```text
Release:            19/19
ASan/UBSan:         17/17
LeakSanitizer:      17/17
ASan/UBSan + seeds: 18/18
```

`ltfa.analysis.failure-propagation` covers:

- every built-in v1 failure-mode result and unresolved extension behavior
- immediate and delayed unavailable and degraded transitions
- two same-time hold-up expirations and deterministic repeat usage
- chained typed dependencies and redundant `any` providers
- entity, endpoint, relation, domain, and declared-mode injection
- availability-relation versus connectivity-only relation failure
- partial/inferred qualification, unknown effects, lost probes, detection gaps,
  and unaffected objects
- event and state-transition budgets
- malformed adjacency, invalid IDs and enums, undersized snapshots, invalid
  available-state injection, and fixed-time overflow

The Release suite also retained static and runtime offline-boundary checks,
complete CLI `strace`, install-tree smoke, and external Graphviz validation.
The implementation adds no file, network, process, environment, database, or
plugin access.
