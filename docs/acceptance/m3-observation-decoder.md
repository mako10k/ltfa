# M3 Observation Decoder Acceptance

Date: 2026-07-31

Task: `M3_OBSERVATION_DECODER`

Status: accepted

## Delivered contract

The public `decode_observation_json_lines` core API decodes an already loaded,
bounded UTF-8 buffer into owned observation values. The decoder is batch
fail-closed: a malformed or over-limit record returns one stable diagnostic
and no observations, including when the error follows valid records.

The implementation accepts only `ltfa.observation/v1`, its eight required
fields, eight optional built-in fields, the five reviewed kinds, the five
reviewed outcomes, and bounded scalar `x-` extensions. It retains input source
spans, exact RFC 3339 source text and decoded components, integer versus real
number representation, optional producer/evidence fields, and sorted
extensions.

Default limits cover the complete input, each physical record, record count,
extension count, extension key, and decoded strings. JSON objects and arrays
cannot be carried as extension values, and secret-bearing extension names are
rejected. The producer remains responsible for redaction because arbitrary
string content cannot be proven non-secret by a schema decoder.

The exact version, field, timestamp, extension, and component boundaries are
recorded in [ADR 0007](../decisions/0007-observation-json-lines-contract.md).

## Requirement evidence

| Requirement | Evidence and boundary |
| --- | --- |
| `REQ-OBS-001..004` | Unit matrices cover JSON Lines framing, UTF-8, required identity/time/validity fields, every built-in kind, and every outcome. |
| `REQ-OBS-005` | ICMP timeout remains the `timeout` observation outcome. The decoder emits no target, device, link, or power inference. |
| `REQ-OBS-006` | The registered `probe_id` is retained. Probe-contract and custom-heartbeat semantic validation belongs to `M3_EVIDENCE_INDEX`. |
| `REQ-OBS-007..008`, `REQ-OBS-013` | Records and their vantage/time/validity are retained without arrival-order overwrite or clock access. Freshness, conflicts, and explicit `analysis_at` evaluation remain the next task. |
| `REQ-OBS-009` | Input, line, count, extension, key, and string bounds fail closed; extension containers and common secret-bearing names are rejected. |
| `REQ-OBS-010..012`, `REQ-SAF-001..002` | The API receives memory only. Static and `strace` gates verify no network, process-launch, plugin, environment, or producer behavior. No active-probe component is added. |
| `REQ-QLT-001` | Success, malformed-input, exact-limit, deterministic-diagnostic, standalone corpus, and Clang libFuzzer coverage are present. |
| `REQ-QLT-005` | `ltfa.observation/v2` and every other unsupported schema fail with `E_OBSERVATION_UNSUPPORTED_SCHEMA`. |
| `REQ-NFR-001`, `REQ-NFR-004`, `REQ-QLT-006` | The C++17 value API hides pinned yyjson; project source builds warnings-as-errors while vendor warnings remain isolated. |

## Negative evidence

The malformed matrix rejects blank records, non-object roots, duplicate and
unknown fields, missing fields, wrong scalar types, invalid IDs, invalid
calendar/time offsets, mismatched metric/state fields, nested extensions,
secret-bearing extension names, control-bearing strings, comments, trailing
commas, NaN, a second JSON value, BOM input, and invalid UTF-8. Exact-limit and
limit-plus-one cases cover input bytes, line bytes, record count, extension
count, extension-key bytes, and decoded-string bytes.

The committed observation fuzz corpus is synthetic and contains no customer
identifier, credential, unrestricted evidence, or live target. A Clang 18
libFuzzer build exposed one pre-existing C++17 portability warning in semantic
DOT output; binding the structured-binding member to an ordinary local
reference resolved it without changing output semantics.

## Validation

```text
Release                                  33/33 passed
ASan + UBSan                             29/29 passed
LeakSanitizer                            29/29 passed
ASan + UBSan standalone fuzz-seed replay 31/31 passed
Clang 18 libFuzzer target build          parser + observation passed
Observation libFuzzer smoke              100 runs passed
```

The Release suite includes installed-tree, static offline-boundary, decoder
runtime `strace`, existing CLI runtime `strace`, and all available Graphviz
checks. The installed and current CLI contract is unchanged and exposes no
observation option; `nm` found no yyjson or observation-decoder symbol in the
CLI executable.

PERT completion and advancement remain separate mutations after this
implementation and acceptance evidence are committed.
