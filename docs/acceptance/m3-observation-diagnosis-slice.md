# M3 Observation Diagnosis Slice Acceptance

Date: 2026-07-31

Task: `M3_ACCEPTANCE`

Status: accepted

## Accepted boundary

Slice 3 is accepted as a one-shot, offline C++17 core path from strict JSON
Lines observations through registered evidence interpretation and bounded
ranked diagnosis to versioned JSON and explanatory DOT. The integrated path
uses the committed synthetic structure-derived fixture and requires no live
site, network, credential, environment value, producer, or adjacent repository.

The machine-output contract is recorded in
[ADR 0011](../decisions/0011-diagnosis-machine-output.md). It deliberately does
not add a CLI command: CLI discovery, input operands, error routing, and human
presentation remain a separately reviewed user-interface change.

## Cross-feature acceptance

`ltfa.acceptance.slice3` runs the complete normalized path and verifies:

- strict decoding of all six synthetic observation records;
- explicit `analysis_at` propagation with no clock default;
- current and stale evidence plus same-probe and cross-vantage conflicts;
- vantage-specific timeout interpretation without a confirmed target-down or
  power-loss field;
- a bounded ten-candidate diagnosis with deterministic rank, tie, score,
  qualification, conflicts, affected objects, and remaining unknowns;
- byte-identical `ltfa.diagnosis/v1` JSON and `ltfa.diagnosis.dot/v1` DOT after
  reversing semantically identical observation arrival order;
- sorted and deduplicated input identities and syntactically valid JSON;
- explicit incomplete status, reason, and resource usage after budget
  exhaustion;
- default omission of restricted identities and permanent omission of optional
  observation detail under both output policies; and
- Graphviz parsing of the selected-finding DOT plus zero runtime network
  syscalls for the complete acceptance executable.

## Requirement trace

| Requirement | Evidence and boundary |
| --- | --- |
| `AC-005`, `REQ-OBS-005` | The current local pass contradicts target-loss hypotheses; timeout remains a registered outcome from its vantage and produces no confirmed target-state field. |
| `AC-006`, `REQ-OBS-007..008` | Current, stale, same-probe, and cross-vantage evidence survive normalization and remain represented in every rendered finding that references them. |
| `AC-007`, `REQ-ANL-011`, `REQ-ANL-015` | Repeated and reverse-arrival analyses emit byte-identical ordered JSON and selected-finding DOT with explicit ranks, ties, and counters. |
| `AC-008`, `REQ-OBS-010..012`, `REQ-SAF-001..002` | Static symbol checks and the dedicated Slice 3 `strace` gate reject network, producer, process, plugin, environment, and secret dependencies. |
| `AC-009`, `REQ-OBS-009`, `REV-009..010` | The structure-derived fixture remains synthetic; renderers never emit optional value/unit/state, duration, producer, evidence-reference, detail, or extension content. |
| `REQ-ANL-010..014` | The candidate set is causal-cone bounded, deterministic, probability-free, qualified partial/inferred, and retains affected objects, conflicts, and unknowns. |
| `REQ-OUT-002..004`, `REQ-OUT-006..007` | Versioned JSON and external-Graphviz DOT identify the operation, explicit reference time, inputs, successful complete/incomplete status, findings, warnings, and usage. |

## Review gates

| Gate | Result |
| --- | --- |
| Strict observation contract | Pass: malformed and unsupported records remain covered by the decoder matrix. |
| Explicit time and freshness | Pass: the fixture reference time is emitted exactly and no system clock is consulted. |
| Evidence versus inference | Pass: observations, assessments, and hypotheses remain distinct fields; no numeric probability or confirmed fault is created. |
| Deterministic machine output | Pass: repeated and reverse-arrival JSON/DOT are byte-identical. |
| Restricted and raw detail | Pass: default restricted identities are omitted; raw optional observation fields are never rendered. |
| Bounded incompleteness | Pass: exhausted hypothesis budget returns `incomplete`, a stable reason, zero generated hypotheses, and usage. |
| DOT interoperability | Pass: external Graphviz accepts the selected-finding graph. |
| Offline execution | Pass: the integrated executable makes no network syscall under `strace`. |

## Validation

The same source state passed the complete repository matrix:

```text
GNU 13.3.0 Release                       42/42 passed
GNU 13.3.0 ASan + UBSan                  34/34 passed
GNU 13.3.0 LeakSanitizer                 34/34 passed
GNU 13.3.0 ASan + UBSan fuzz-seed replay 36/36 passed
Clang 18.1.3 ASan + UBSan                34/34 passed
Clang 18.1.3 libFuzzer core and targets  built
```

Release includes the dedicated Slice 3 runtime boundary and external Graphviz
render. The sanitizer configurations retain the static offline-symbol gate,
complete integration test, install-tree smoke, and Graphviz parse. Clang also
compiles the new renderer into the isolated fuzz core used by both existing
libFuzzer targets.

## Deferred work

Acceptance reaches the Slice 3 core boundary only. It does not add or authorize
a diagnosis CLI, standard-input workflow, live probe, monitoring adapter,
automatic next observation, bounded cut analysis, tag, release, publication,
or installation. Slice 4 remains responsible for bounded cuts and
next-useful-observation recommendations.
