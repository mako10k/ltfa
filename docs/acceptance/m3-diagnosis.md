# M3 Bounded Diagnosis Acceptance

Date: 2026-07-31

Task: `M3_DIAGNOSIS`

Status: accepted

## Delivered contract

The public `diagnose_observations` API turns the latest registered evidence into
a bounded causal cone, simulates declared-mode and generic object single-failure
hypotheses, and returns deterministic dense-ranked findings. The exact rules
are recorded in [ADR 0009](../decisions/0009-bounded-deterministic-diagnosis.md).

The result exposes affected objects, every assessed latest observation,
conflicting evidence, remaining unknown objects and evidence, topology
completeness, evidence quality, usage counters, and stable incomplete reasons.
It has no numeric probability or unqualified confirmed-fault classification.

An ICMP timeout only seeds the target/vantage causal cone. In the focused
multi-vantage test, simulated loss of the failing vantage outranks target-down
hypotheses because the latter contradict the other vantage's current pass. Both
sides of the cross-vantage conflict remain attached to every finding.

## Requirement evidence

| Requirement | Evidence and boundary |
| --- | --- |
| `REQ-ANL-010` | Tests prove that the cone contains the failing probe's target, vantage, and reverse availability provider while excluding an unrelated object and the non-failing vantage. |
| `REQ-ANL-011` | Focused tests cover contradiction-first ranking, explained and unexplained non-pass counters, declared-mode specificity, stable-ID order, and dense explicit ties. |
| `REQ-ANL-012` | Neither the public API nor its finding types contain probability or numeric confidence fields. |
| `REQ-ANL-013` | Findings carry affected objects, assessed evidence, conflict indices, scope qualification, detection gaps, unknown objects, and unknown or unexplained evidence. |
| `REQ-ANL-014` | The leading vantage hypothesis retains `partial` completeness and `inferred` evidence quality from its modeled scope. |
| `REQ-ANL-015`, `AC-007` | Reversing observation arrival order produces the same candidate count, ordered stable IDs, ranks, ties, and score counters for identical semantic evidence and budgets. Machine rendering is integrated by the later `M3_ACCEPTANCE` task. |
| `REQ-OBS-005`, `AC-005` | Timeout creates only labeled hypotheses; a current pass from another vantage contradicts target-down hypotheses, and no direct target-state field exists. |
| `REQ-OBS-008`, `AC-006` | Both observations in the fresh cross-vantage conflict remain explicitly indexed in each finding. |
| `REQ-SAF-001..002`, `AC-008` | The API receives owned values only. Static boundary checks plus diagnosis-specific runtime `strace` cover the offline core. |

## Focused coverage

The diagnosis executable covers:

- target, vantage, and reverse-dependency causal-cone membership
- exclusion of unrelated topology and a passing-only vantage from candidate generation
- declared failure-mode and generic object hypotheses
- contradiction-first multi-vantage ranking
- explicit dense ties and stable-ID ordering
- partial/inferred scope qualification
- stale evidence seeding without current-state scoring
- fresh pass-only evidence producing no hypothesis
- causal-cone, hypothesis, simulation, evidence-comparison, and nested propagation bounds
- no partially simulated or partially scored finding after exhaustion
- invalid evidence-index rejection and stable public enum names
- reversed observation input order

## Validation

The complete validation matrix passes:

```text
Release                                  37/37 passed
ASan + UBSan                             31/31 passed
LeakSanitizer                            31/31 passed
ASan + UBSan standalone fuzz-seed replay 33/33 passed
Clang 18 ASan + UBSan                    31/31 passed
Clang 18 libFuzzer core and targets      built
Observation libFuzzer smoke              100 mutation runs passed
```

Release includes the diagnosis-specific runtime `strace`, existing CLI and core
offline checks, installed-tree smoke, and all available Graphviz checks. The
Clang libFuzzer build compiles diagnosis into the isolated fuzz core. The
observation smoke used a disposable copied corpus, reached 100 actual mutation
runs without a crash or sanitizer finding, and removed the temporary corpus.

The CLI contract remains unchanged and exposes no observation or diagnosis
option. The later integrated Slice 3 acceptance adds public core JSON/DOT
renderers without changing that CLI boundary.
