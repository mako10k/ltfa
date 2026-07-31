# M3 Evidence Index Acceptance

Date: 2026-07-31

Task: `M3_EVIDENCE_INDEX`

Status: accepted

## Delivered contract

The public `build_evidence_index` API resolves decoded observations against a
normalized model and an explicit `analysis_at`. It fails closed on an invalid
reference time, a future observation, an unresolved probe, an ambiguous probe,
or any mismatch in registered target, vantage, kind, validity, or required
custom-heartbeat outcome mapping.

The owned result keeps observations in deterministic group/time order. Each
probe/vantage group exposes all records at its newest instant and all older
history. Freshness is attached to every record, while selection remains
separate so an older in-window record cannot replace the newest result.
Same-time signal disagreements and fresh disagreements across vantages are
explicit conflict objects that point to all participating records.

ICMP observations are classified only as vantage-specific reachability
evidence. No evidence-index type can assert target unavailability, device
failure, link failure, or power loss. The exact rules are recorded in
[ADR 0008](../decisions/0008-observation-evidence-index.md).

## Requirement evidence

| Requirement | Evidence and boundary |
| --- | --- |
| `REQ-OBS-002..004` | The index retains the complete decoded record and validates it against the registered probe contract. |
| `REQ-OBS-005`, `AC-005` | Every ICMP record has `vantage-reachability` meaning; timeout creates no target-state assertion. |
| `REQ-OBS-006` | Custom-heartbeat observations require a registered custom-heartbeat probe with non-empty `x-outcome-map` semantics. |
| `REQ-OBS-007` | Exact-boundary and boundary-plus-one-nanosecond tests distinguish current from stale while retaining stale history. |
| `REQ-OBS-008`, `AC-006` | Same-time and cross-vantage conflicts retain all records; reversed arrival order produces the same semantic order and conflict. |
| `REQ-OBS-013` | `analysis_at` is required, retained, strictly parsed, and never sourced from an implicit clock. Future observations fail explicitly. |
| `REQ-ANL-015`, `AC-007` | UTC-offset normalization, deterministic sorting, and stable enum names establish the evidence ordering consumed by the later `M3_ACCEPTANCE` byte-stable renderers. |
| `REQ-OBS-010..012`, `REQ-SAF-001..002` | The APIs receive values only. Static and runtime gates cover the offline boundary; no producer, network, process, plugin, or credential path exists. |

## Focused coverage

The evidence-index executable covers:

- newest selection and retained stale history
- exact freshness expiry and one-nanosecond-later staleness
- equivalent instants written with different RFC 3339 offsets
- same-probe outcome and metric-value conflicts
- fresh cross-vantage disagreement and stale-vantage exclusion
- reversed input arrival order
- registered custom-heartbeat mapping
- unresolved probe and target, vantage, kind, and validity mismatches
- future observation and invalid explicit reference time
- empty observation input and stable public enum names

## Validation

```text
Release                                  35/35 passed
ASan + UBSan                             30/30 passed
LeakSanitizer                            30/30 passed
ASan + UBSan standalone fuzz-seed replay 32/32 passed
Clang 18 evidence and libFuzzer builds   passed
Observation libFuzzer smoke              100 runs passed
```

The Release suite includes installed-tree, static offline-boundary,
evidence-index runtime `strace`, observation-decoder runtime `strace`, existing
CLI runtime `strace`, and all available Graphviz checks. The Clang 18 build
compiled the evidence index into both the normal core and the parser and
observation libFuzzer core. The libFuzzer smoke used a disposable working
corpus; generated mutations were removed and no corpus change is retained.

The CLI contract remains unchanged and exposes no observation option. PERT
completion and advancement remain separate mutations after this implementation
and acceptance evidence are committed.
