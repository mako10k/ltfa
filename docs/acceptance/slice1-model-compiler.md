# Slice 1 Model Compiler Acceptance

Status: accepted on 2026-07-28

## Accepted Boundary

Slice 1 provides a one-shot, offline path from explicitly rooted LTFA DSL files
to a validated normalized model and deterministic human, JSON, or DOT output:

```bash
ltfa model --model-root tests/fixtures/slice1 --input main.ltfa --format json
```

Exit status `0` means the model loaded and validated. Status `2` is CLI misuse,
`3` is an input, include, or parse failure, `4` is model validation failure, and
`5` is output failure. Diagnostics use
`file:line:column: severity CODE: explanation`. The CLI does not analyze
topology, execute input, open a network connection, or invoke Graphviz.

## Requirement Trace

| Requirement | Acceptance evidence |
| --- | --- |
| `REQ-MOD-001..018` | `model_contract_test.cpp`, `model_compiler_test.cpp`, and the Slice 1 fixture cover typed scopes, entities, endpoints, relations, requirements, domains, failures, probes, metadata, and deterministic indices. |
| `REQ-DSL-001..008` | Lexer, parser, include, hardening, and CLI tests cover DSL v1, exact diagnostics, root-safe includes, malformed input, and configured limits. |
| `REQ-OUT-001..008` | Golden output tests and `ltfa.cli.model.acceptance` verify result-first human output, versioned JSON, DOT, explicit sorted inputs, stable repeated output, and distinct process errors. |
| `REQ-SAF-001..005`, `REQ-SAF-008` | Restricted-output tests, source/import audits, and `strace` tests verify fail-closed local input and no network, command, plugin, or environment access. |
| `REQ-NFR-001..002`, `REQ-NFR-005..006` | C++17 one-shot CLI, no database, and optional external Graphviz tests preserve the approved runtime boundary. |
| `REQ-QLT-001`, `REQ-QLT-003..004`, `REQ-QLT-006` | Malformed matrices, fuzz corpus/libFuzzer target, golden tests, warnings-as-errors, and ASan/UBSan/LSan presets provide the required verification layers. |
| `AC-001` | `tests/fixtures/slice1/` validates power, network, serial, contact, current, VLAN/L3, tunnel, VPC/VNET, and service dependencies through the real CLI. |
| `AC-002`, `AC-007..008` | Invalid CLI fixtures retain exact codes and locations; repeated JSON/DOT is byte-stable; static and runtime checks require neither network nor credentials. |

`AC-003..006` and `AC-009..010` depend on later assurance, observation,
diagnosis, redacted-fixture, and benchmark slices. This acceptance does not
claim those criteria.

## Verification

Run all committed configurations:

```bash
cmake --preset release && cmake --build --preset release && ctest --preset release
cmake --preset asan-ubsan && cmake --build --preset asan-ubsan && ctest --preset asan-ubsan
cmake --preset lsan && cmake --build --preset lsan && ctest --preset lsan
cmake --preset fuzz-seeds && cmake --build --preset fuzz-seeds && ctest --preset fuzz-seeds
```
