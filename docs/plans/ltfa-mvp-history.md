# LTFA MVP Plan History

## 2026-07-28 — M1_MODEL_CONTRACT

Status: accepted

The first critical-path task established the public, value-oriented model and
diagnostic contract without adding DSL parsing, external I/O, or network
behavior.

Delivered:

- strong, non-interchangeable index IDs and source spans
- reviewed default model and input limits
- stable diagnostic code families
- typed scope, entity, endpoint, relation, requirement, failure, and probe
  records
- deterministic names for every public model and diagnostic enum
- dependency-free table-driven contract tests registered with CTest

Verification:

```text
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

The Release build passed with warnings treated as errors. All four tests
passed, including `ltfa.model.contract`. The pre-advance plan digest was
`sha256:37ef9eb6bd57689ed758c052b75692959d5d08ff1ebb78d837501f76e7fa277d`.

## 2026-07-28 — M1_BOUNDED_LEXER

Status: accepted

The second critical-path task added bounded, source-aware lexical analysis. It
does not parse statement grammar, resolve includes, expand input text, or
perform external I/O.

Delivered:

- strict UTF-8 validation with BOM, overlong, surrogate, truncated, and
  out-of-range rejection
- physical-line tokenization for bare values, quoted strings, `=`, `->`, and
  `--`
- comments recognized only outside quoted strings
- stable file, line, Unicode-column, and byte-offset spans
- configured source-file and line-byte limits with stable diagnostics
- fail-closed results that expose no partial token stream after an error
- table-driven tests for CRLF, Japanese labels, opaque shell-like text,
  malformed strings, controls, limits, and UTF-8 boundaries

Verification:

```text
cmake -S . -B /tmp/ltfa-lexer-build.HkF5Lo -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/ltfa-lexer-build.HkF5Lo
ctest --test-dir /tmp/ltfa-lexer-build.HkF5Lo --output-on-failure
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir /tmp/ltfa-lexer-san.IX9LuK --output-on-failure
```

The clean GNU 13.3.0 Release build passed with warnings treated as errors, and
all five tests passed. The AddressSanitizer/UBSan build also passed all five
tests with leak detection disabled; LeakSanitizer itself was unavailable under
the ptrace-based execution environment and is not claimed as verified. The
pre-advance plan digest was
`sha256:447836960e02a9a40f71ceb92fe24e236d54b617c788f9334146668899104525`.

## 2026-07-28 — M1_DSL_PARSER

Status: accepted

The third critical-path task converts bounded lexer output into typed DSL v1
records. It preserves include paths but does not resolve them, intern IDs,
resolve references, or compile the normalized model.

Delivered:

- exact `ltfa 1` header validation
- typed records for scope, entity, endpoint, connect, relate, require, failure,
  probe, and include statements
- required owner, direction, provider, target, and vantage fields
- statement-specific known-attribute allowlists and repeatable failure domains
- preserved `x-` extension attributes with lexical value kinds and source spans
- fail-closed stable diagnostics for unknown statements, fields, duplicates,
  missing fields, and invalid values
- configured attribute-count and string-byte limits

Verification:

```text
cmake -S . -B /tmp/ltfa-parser-release.NezL4N -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/ltfa-parser-release.NezL4N
ctest --test-dir /tmp/ltfa-parser-release.NezL4N --output-on-failure
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir /tmp/ltfa-parser-san.RbfP3u --output-on-failure
```

The clean GNU 13.3.0 Release build passed with warnings treated as errors, and
all six tests passed. The AddressSanitizer/UBSan build also passed all six tests
with leak detection disabled; LeakSanitizer is not claimed as verified. The
pre-advance plan digest was
`sha256:db3fdc37e402ddb0763f1d1b2031862f08731b484bf12f271a62eae8b8d81313`.

## 2026-07-28 — M1_SAFE_INCLUDES

Status: accepted

The fourth critical-path task adds bounded local include loading. It expands
parsed statements but does not intern IDs, resolve object references, validate
relation semantics, or compile the normalized model.

Delivered:

- explicit model-root and entry-file API with canonical source-path mapping
- depth-first textual expansion while preserving per-file source spans
- relative-only paths with URI, environment expansion, absolute path, lexical
  escape, and symlink escape rejection
- active-stack cycle detection with primary and secondary source locations
- depth 16, per-file 16 MiB, and total-load 64 MiB defaults checked before
  allocation
- exact-read checks that fail closed when a file changes during loading
- multi-scope validation requiring explicit entity `scope=` attributes
- source tables retained for nested-file diagnostics without exposing partial
  statements

Verification:

```text
cmake -S . -B /tmp/ltfa-includes-release.k4Cj1q -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/ltfa-includes-release.k4Cj1q
ctest --test-dir /tmp/ltfa-includes-release.k4Cj1q --output-on-failure
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir /tmp/ltfa-includes-san.rSzRVs --output-on-failure
```

The clean GNU 13.3.0 Release build passed with warnings treated as errors, and
all seven tests passed. The AddressSanitizer/UBSan build also passed all seven
tests with leak detection disabled; LeakSanitizer is not claimed as verified.
A static source scan found no socket, shell-execution, or environment-access
calls in the public or core source trees. The pre-advance plan digest was
`sha256:5f725a093a61374f7b0cb8af96bb47b43bb8eb4acad0e224aa449529d5441660`.

## 2026-07-28 — M1_MODEL_COMPILER

Status: accepted

The fifth critical-path task compiles expanded DSL records into a validated,
deterministic normalized graph. It does not emit model views or run topology or
availability analysis.

Delivered:

- globally unique stable IDs, deterministic string interning, and external-ID
  ordered normalized vectors
- resolved scopes, endpoint owners, object references, network spaces, and
  many-to-many failure-domain membership
- deterministic length-prefixed relation and requirement-group IDs where DSL
  v1 has no explicit ID field
- endpoint, direction, plane, same-layer network, and cross-layer validation
- typed bounded attributes with inert `x-` preservation and versioned built-in
  failure-mode metadata
- merged `all` and `any` requirement groups with millisecond durations
- combined implicit and explicit availability dependencies with cycle rejection
- relation-kind and availability forward/reverse adjacency ranges
- fail-closed duplicate, unresolved-reference, semantic, overflow, and limit
  diagnostics without partial normalized models

Verification:

```text
cmake -S . -B /tmp/ltfa-model-release.AWUUlC -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/ltfa-model-release.AWUUlC
ctest --test-dir /tmp/ltfa-model-release.AWUUlC --output-on-failure
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir /tmp/ltfa-model-san.Y2Hh1C --output-on-failure
```

The clean GNU 13.3.0 Release build passed with warnings treated as errors, and
all eight tests passed. The AddressSanitizer/UBSan build also passed all eight
tests with leak detection disabled; LeakSanitizer is not claimed as verified.
A static source scan found no socket, shell-execution, environment-access,
database, or plugin-loading calls in the added compiler path. The pre-advance
plan digest was
`sha256:87711f9b4e7fe11471884d796224ef338289bf74d61712050315853ff0bf58a6`.

## 2026-07-28 — M1_MODEL_OUTPUT

Status: accepted

The sixth critical-path task renders a compiled model through deterministic
value-oriented core APIs. It does not add topology analysis, invoke Graphviz,
or select files through the CLI.

Delivered:

- result-first human summaries with scope completeness, evidence quality,
  unknowns, explicit inputs, and omission counts
- a byte-stable `ltfa.model/v1` JSON envelope with stable fields, sorted object
  arrays and attributes, explicit input sets, and no implicit timestamp
- escaped DOT text with versioned graph metadata, stable plane and relation
  styles, scope/plane/layer/failure-domain grouping, filters, focus, and
  highlights
- default omission of restricted objects and evidence references, including
  transitive suppression through restricted network-space identity
- an explicit opt-in policy for restricted JSON and human artifacts while DOT
  labels remain evidence-free
- golden human, JSON, and DOT tests plus declaration-order, input-order,
  escaping, view-selection, and restricted-data fixtures

Verification:

```text
cmake -S . -B /tmp/ltfa-output-release.6bc1S2 -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/ltfa-output-release.6bc1S2
ctest --test-dir /tmp/ltfa-output-release.6bc1S2 --output-on-failure
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir /tmp/ltfa-output-san.6pmwcq --output-on-failure
```

The clean GNU 13.3.0 Release build passed with warnings treated as errors, and
all nine tests passed. The AddressSanitizer/UBSan build also passed all nine
tests with leak detection disabled; LeakSanitizer is not claimed as verified.
The added path contains no socket, shell-execution, environment-access, file
I/O, database, plugin, or Graphviz linkage. The host had no `dot` executable,
so no external rendering result is claimed. The pre-advance plan digest was
`sha256:7b1e139ca0b7f41366f163c9f73b6a1fa6e38b3bf3afac0c68c2297ddb796315`.

## 2026-07-28 — M1_PARSER_HARDENING

Status: accepted

The seventh critical-path task closes the first parser slice with explicit
malformed-input, resource-limit, fuzz, sanitizer, and offline-boundary
verification. It does not add observation decoding, analysis, or network
behavior.

Delivered:

- deterministic malformed parser and compiler matrices with exact diagnostic
  codes and source locations and no partial document or model
- exact-boundary and limit-plus-one checks for source, line, attribute, object,
  relation, and total byte limits, complementing existing include limits
- a bounded parser/compiler/output fuzz harness, four synthetic corpus seeds,
  standalone replay, and an isolated Clang/libFuzzer-instrumented core
- committed Release, ASan/UBSan, LSan, and fuzz-seed CMake presets
- static source and binary-import checks for network, process-launch,
  plugin-loading, environment-access, and credential-shaped literals
- Linux `strace` verification that the parser integration test performs no
  network syscall
- optional external Graphviz validation that renders generated LTFA DOT to SVG
  without linking Graphviz into the core

Verification:

```text
cmake --preset release && cmake --build --preset release && ctest --preset release
cmake --preset asan-ubsan && cmake --build --preset asan-ubsan && ctest --preset asan-ubsan
cmake --preset lsan && cmake --build --preset lsan && ctest --preset lsan
cmake --preset fuzz-seeds && cmake --build --preset fuzz-seeds && ctest --preset fuzz-seeds
build/libfuzzer/tests/ltfa_parser_fuzz <temporary-corpus> -runs=1000 -max_len=65536
```

The GNU 13.3.0 Release and fuzz-seed suites passed all 13 tests. The GNU
ASan/UBSan and independent LSan suites passed all 12 applicable tests. Clang
18.1.3 libFuzzer completed 1,000 runs without a crash or sanitizer finding,
ending at 2,694 covered edges and 4,738 features with 137 MiB peak RSS. The
Release boundary test observed zero network syscalls, and Graphviz 2.43.0
accepted the generated DOT and produced SVG. The pre-advance plan digest was
`sha256:4b421b8988decb721156b94d6e419d1d6f8978de9e8de1fabec80ca57ef982d9`.

## 2026-07-28 — M1_ACCEPTANCE

Status: accepted

The eighth critical-path task accepts Slice 1 through the real one-shot CLI and
a repository-local requirement trace. It does not add assurance analysis,
observation decoding, live probes, or Graphviz linkage.

Delivered:

- `ltfa model --model-root <directory> --input <file> --format
  human|json|dot` for bounded local load, validation, normalization, and output
- stable exit statuses for usage, input/parse, validation, and output failures,
  with relative file, line, column, diagnostic code, and related location
- default restricted-object omission and the explicit
  `--include-restricted` policy
- a five-file synthetic fixture covering power, network, serial, contact,
  analog current, VLAN/L3, tunnel, VPC/VNET, and service dependencies
- end-to-end checks for deterministic JSON/DOT, explicit sorted inputs, human
  result ordering, invalid models, escaping includes, CLI misuse, and
  restricted output
- a Slice 1 requirement-to-test trace that explicitly leaves later assurance,
  observation, diagnosis, redacted-fixture, and benchmark criteria open
- `strace` evidence that the complete CLI path performs no network syscall

Verification:

```text
cmake --preset release && cmake --build --preset release && ctest --preset release
cmake --build --preset asan-ubsan && ctest --preset asan-ubsan
cmake --build --preset lsan && ctest --preset lsan
cmake --build --preset fuzz-seeds && ctest --preset fuzz-seeds
cmake --build build/libfuzzer
ASAN_OPTIONS=detect_leaks=0 ctest --test-dir build/libfuzzer --output-on-failure
ltfa model --model-root tests/fixtures/slice1 --input main.ltfa --format dot | dot -Tsvg
```

The GNU 13.3.0 Release suite passed all 15 tests, including both runtime
offline checks. GNU ASan/UBSan and independent LSan passed all 13 applicable
tests, and the fuzz-seed configuration passed all 14. Clang 18.1.3 ASan/UBSan
also passed all 13 applicable tests. The accepted fixture normalized to 15
entities, 14 endpoints, and 15 relations; repeated JSON and DOT were
byte-stable. Graphviz 2.43.0 accepted DOT emitted through the CLI. The
pre-advance plan digest was
`sha256:58e6c9e44ca88b3188ffc06f09027353c6fbd6f9b765921a99e25ad0b2adfd67`.

## 2026-07-29 — M2_REQUIREMENT_SEMANTICS

Status: accepted

The first Slice 2 critical-path task adds pure availability semantics over the
normalized requirement groups and failure domains already produced by Slice 1.
It does not schedule propagation, interpret failure modes, expose assurance
through the CLI, or run path, impact, articulation, bridge, or SPOF analysis.

Delivered:

- model-sized entity and endpoint availability snapshots
- exact four-state `all` and `any` truth tables with conservative unknown
  handling
- stable multiple-group consumer evaluation with fixed hold-up metadata
- consumer completeness and evidence-quality qualification
- stable failure-domain expansion with conservative member qualification
- transactional failure application across entity and endpoint domain members
- invalid-ID, malformed-range, undersized-state, and no-partial-mutation
  handling

Verification:

```text
cmake --preset release && cmake --build --preset release && ctest --preset release
cmake --preset asan-ubsan && cmake --build --preset asan-ubsan && ctest --preset asan-ubsan
cmake --preset lsan && cmake --build --preset lsan && ctest --preset lsan
cmake --preset fuzz-seeds && cmake --build --preset fuzz-seeds && ctest --preset fuzz-seeds
```

The GNU 13.3.0 Release suite passed all 17 tests, including static and runtime
offline checks, install-tree smoke, and Graphviz validation. ASan/UBSan and
independent LSan passed all 15 applicable tests, and the fuzz-seed configuration
passed all 16. The focused test exhaustively checked all 16 ordered state pairs
for both requirement modes and verified multiple groups, hold-up retention,
scope qualification, overlapping domains, endpoint membership, stable order,
and mutation-free invalid input. The pre-advance plan digest was
`sha256:a1d480e80c247736c425127c4003eddbe481bd1629d87e986cf444495037633d`.

## 2026-07-29 — M2_PATH_IMPACT

Status: accepted

The second Slice 2 core task adds filtered reachability, deterministic
representative paths, and modeled connectivity-removal impact over the
normalized adjacency indexes. It does not implement weighted paths, failure
propagation, articulation, bridge, cut-set, or SPOF classification and does not
change the accepted `v0.1.0` CLI contract.

Delivered:

- bounded breadth-first traversal without copying the normalized graph
- forward, reverse, and undirected reachability
- deterministic representative unweighted paths
- plane, layer, scope, failure-domain, relation-kind, and availability filters
- entity, endpoint, relation, and many-to-many failure-domain removal impact
- stable external-ID ordering and explicit traversal usage
- complete, incomplete, and invalid outcomes
- completeness, evidence-quality, and unknown-availability qualification

Verification:

```text
cmake --preset release && cmake --build --preset release && ctest --preset release
cmake --preset asan-ubsan && cmake --build --preset asan-ubsan && ctest --preset asan-ubsan
cmake --preset lsan && cmake --build --preset lsan && ctest --preset lsan
cmake --preset fuzz-seeds && cmake --build --preset fuzz-seeds && ctest --preset fuzz-seeds
```

The GNU 13.3.0 Release suite passed all 18 tests, including static and runtime
offline checks, install-tree smoke, and Graphviz validation. ASan/UBSan and
independent LSan passed all 16 applicable tests, and the fuzz-seed configuration
passed all 17. The focused test covered stable repeat results, same-layer and
cross-layer paths, every declared view filter, all traversal directions,
entity, endpoint, relation, and domain removal, redundant paths, common-cause
loss, qualification, budgets, and invalid input. The pre-advance plan digest
was `sha256:e903ebbe44e22ae59cb26c74c6ca095dc539eaa9ac6d88ba093ee3d8cf519dbc`.

## 2026-07-29 — M2_FAILURE_PROPAGATION

Status: accepted

The third Slice 2 core task adds deterministic propagation of one object,
endpoint, relation, failure-domain, or declared failure-mode injection through
the normalized acyclic availability graph. It retains immediate, degraded,
delayed, unaffected, detection-gap, lost-probe, and unknown outcomes without
adding observation inference or changing the accepted `v0.1.0` CLI contract.

Delivered:

- fixed built-in failure-mode result semantics and conservative `x-` unknown
- direct typed dependency and `all`/`any` requirement-group propagation
- fixed hold-up with time-and-stable-ID event ordering
- atomic failure-domain injection and relation-failure inputs
- fixed-point final snapshots and stable event timelines
- immediate and delayed unavailable or degraded classifications
- unaffected, unknown, lost-probe, and direct-probe detection-gap findings
- completeness and evidence-quality qualification
- explicit event and state-transition budgets with fail-closed validation

Verification:

```text
cmake --preset release && cmake --build --preset release && ctest --preset release
cmake --preset asan-ubsan && cmake --build --preset asan-ubsan && ctest --preset asan-ubsan
cmake --preset lsan && cmake --build --preset lsan && ctest --preset lsan
cmake --preset fuzz-seeds && cmake --build --preset fuzz-seeds && ctest --preset fuzz-seeds
```

The GNU 13.3.0 Release suite passed all 19 tests, including static and runtime
offline checks, install-tree smoke, and Graphviz validation. ASan/UBSan and
independent LSan passed all 17 applicable tests, and the fuzz-seed configuration
passed all 18. The focused test covered every built-in failure mode, immediate
and delayed unavailable and degraded transitions, simultaneous hold-up expiry,
stable repeat output and usage, redundancy, every MVP injection target kind,
unknown extensions, qualification, probes, resource budgets, malformed input,
and time overflow. The pre-advance plan digest was
`sha256:44e356227f936c6e9088d6c814d9cfa5d1a1a26af5cafc07fa9275ecabd6b31f`.

## 2026-07-29 — M2_SPOF_ANALYSIS

Status: accepted

The fourth Slice 2 core task adds iterative linear articulation and bridge
analysis plus qualified source-to-target SPOF checks over an explicit
undirected view. It distinguishes verified findings, model-limited findings,
and surviving paths without turning a selected-view result into a global
safety claim.

Delivered:

- deterministic iterative Tarjan articulation and bridge analysis
- parallel-relation identity handling without false bridges
- automatic articulation, bridge, and shared failure-domain candidates
- explicit object, relation, and failure-domain candidate checks
- baseline and retained path evidence
- affected objects and targets, common domains, and common-cause members
- `confirmed-spof`, `model-spof`, and `not-a-spof-in-view`
- completeness, evidence-quality, unknown-state, and filter-exclusion
  qualification
- candidate, visited-object, and examined-relation budgets
- malformed-input and terminal-candidate fail-closed handling

Verification:

```text
cmake --preset release && cmake --build --preset release && ctest --preset release
cmake --preset asan-ubsan && cmake --build --preset asan-ubsan && ctest --preset asan-ubsan
cmake --preset lsan && cmake --build --preset lsan && ctest --preset lsan
cmake --preset fuzz-seeds && cmake --build --preset fuzz-seeds && ctest --preset fuzz-seeds
```

The GNU 13.3.0 Release suite passed all 20 tests, including static and runtime
offline checks, install-tree smoke, and Graphviz validation. ASan/UBSan and
independent LSan passed all 18 applicable tests, and the fuzz-seed configuration
passed all 19. The focused test covered exact structural results, parallel
relations, redundant object and edge removal, shared-domain common-cause loss,
stable repeat output and usage, all three classifications, qualification,
resource budgets, and malformed input. The pre-advance plan digest was
`sha256:7099c1f3c176b5c591bca82939d9e8f5d48e8f7fb9a7f512b259ff6b2323a126`.

## 2026-07-29 — M2_ACCEPTANCE

Status: accepted

The Slice 2 acceptance task combines the four assurance-core increments in one
synthetic, reviewable, model-root-loaded fixture. It closes the offline
assurance implementation boundary without exposing a new CLI command or
starting observation, diagnosis, or cut-set work.

Accepted:

- a redundant single relation retains a selected path
- shared route members produce a common-cause SPOF
- one breaker produces exact immediate and 30-second delayed service loss
- independent backup power survives one breaker while a shared-domain loss
  removes both providers
- verified, partial, inferred, and view-limited SPOF claims remain distinct
- repeated analysis facts, work usage, JSON, and DOT remain deterministic
- path, SPOF, and propagation budgets return explicit incomplete results
- the integrated executable makes no network syscall and uses no secret

Verification:

```text
cmake --preset release && cmake --build --preset release && ctest --preset release
cmake --preset asan-ubsan && cmake --build --preset asan-ubsan && ctest --preset asan-ubsan
cmake --preset lsan && cmake --build --preset lsan && ctest --preset lsan
cmake --preset fuzz-seeds && cmake --build --preset fuzz-seeds && ctest --preset fuzz-seeds
```

The GNU 13.3.0 Release suite passed all 22 tests. ASan/UBSan and independent
LSan passed all 19 applicable tests, and the fuzz-seed configuration passed all
20. Release includes a dedicated `strace` check for the Slice 2 integration
executable. The pre-advance plan digest was
`sha256:46217f47a88d7745fcdb46c0e341e06231392d85c5d502a9ad15e56fbd01479d`.

## 2026-07-31 — M3_OBSERVATION_DECODER

Status: accepted

The first Slice 3 implementation task adds a strict, offline JSON Lines decoder
for observation records. It establishes the ingestion boundary without adding a
CLI surface, evidence reconciliation, diagnosis, or live collection.

Delivered:

- a public C++17 observation-record API and deterministic yyjson-backed decoder
- one complete JSON value per non-empty line with exact line-number diagnostics
- strict required-field, timestamp, enum, identifier, and scalar validation
- preserved observation time, collector time, vantage, confidence, source, and
  probe contract fields
- bounded input, line, string, member, depth, and record limits
- duplicate-key rejection and explicit rejection of unsupported nested values
- focused malformed-input, boundary, repeatability, and resource-limit tests
- a dedicated libFuzzer target with a checked-in seed corpus
- an offline `strace` check and documented acceptance evidence

Verification:

```text
cmake --preset release && cmake --build --preset release && ctest --preset release
cmake --preset asan-ubsan && cmake --build --preset asan-ubsan && ctest --preset asan-ubsan
cmake --preset lsan && cmake --build --preset lsan && ctest --preset lsan
cmake --preset fuzz-seeds && cmake --build --preset fuzz-seeds && ctest --preset fuzz-seeds
```

The GNU 13.3.0 Release suite passed all 33 tests. ASan/UBSan and independent
LSan passed all 29 applicable tests, and the fuzz-seed configuration passed all
31. Clang 18 built both libFuzzer targets, and the observation decoder completed
a 100-run smoke. The dedicated decoder executable made no network syscall. The
pre-advance plan digest was
`sha256:b05033dd9254bd075c3ae2c64ccb7797dd3fc7d3c7c8eb7a266a0b96b62c91a5`.

## 2026-07-31 — M3_EVIDENCE_INDEX

Status: accepted

The second Slice 3 task adds a deterministic, offline evidence index between
strict observation decoding and later graph-aware diagnosis. It resolves every
record against registered probe semantics and an explicit analysis reference
time without adding a clock default, CLI surface, producer, or target-state
inference.

Delivered:

- public C++17 evidence-index and strict timestamp parsing APIs
- exact registered probe, target, vantage, kind, and validity validation
- required `x-outcome-map` registration for custom-heartbeat probes
- normalized RFC 3339 instant comparison and explicit future-record failure
- exact-boundary current/stale classification with retained stale history
- deterministic newest-record selection with all same-time records preserved
- outcome, metric-value, and discrete-state conflict detection
- fresh cross-vantage conflict preservation independent of arrival order
- explicit ICMP `vantage-reachability` meaning without device-down inference
- focused offline and malformed-contract tests plus a runtime `strace` gate

Verification:

```text
cmake --preset release && cmake --build --preset release && ctest --preset release
cmake --preset asan-ubsan && cmake --build --preset asan-ubsan && ctest --preset asan-ubsan
cmake --preset lsan && cmake --build --preset lsan && ctest --preset lsan
cmake --preset fuzz-seeds && cmake --build --preset fuzz-seeds && ctest --preset fuzz-seeds
```

The GNU 13.3.0 Release suite passed all 35 tests. ASan/UBSan and independent
LSan passed all 30 applicable tests, and the fuzz-seed configuration passed all
32. Clang 18 built the evidence test and both libFuzzer targets; the observation
fuzzer completed a 100-run disposable-corpus smoke. Static and runtime checks
confirmed the offline boundary, and the CLI exposes no observation surface. The
pre-advance plan digest was
`sha256:efbc2ebec5e68bc9f2a63e773a7bfae075e372c7c9eb79d1fbe7db0653d83fa2`.

## 2026-07-31 — M3_DIAGNOSIS

Status: accepted

The third Slice 3 task adds bounded, deterministic diagnosis over the strict
evidence index and normalized availability graph. It produces explicitly
labeled single-failure hypotheses without adding a CLI surface, live probe,
numeric probability, or confirmed target-state claim.

Delivered:

- public C++17 diagnosis result, finding, evidence-assessment, and budget APIs
- stable reverse-dependency causal cones seeded only by failed or stale probes
- declared failure-mode and generic object single-failure hypotheses
- bounded deterministic propagation and current-evidence comparison
- contradiction-first lexicographic ranking with stable dense ties
- retained cross-vantage conflicts, affected objects, and remaining unknowns
- scope completeness and evidence-quality qualification on every finding
- explicit causal, hypothesis, simulation, comparison, and propagation limits
- fail-closed validation for malformed model and evidence-index contracts
- focused offline, repeatability, ranking, stale-evidence, and budget tests

Verification:

```text
cmake --preset release && cmake --build --preset release && ctest --preset release
cmake --preset asan-ubsan && cmake --build --preset asan-ubsan && ctest --preset asan-ubsan
cmake --preset lsan && cmake --build --preset lsan && ctest --preset lsan
cmake --preset fuzz-seeds && cmake --build --preset fuzz-seeds && ctest --preset fuzz-seeds
```

The GNU 13.3.0 Release suite passed all 37 tests. ASan/UBSan and independent
LSan passed all 31 applicable tests, and the fuzz-seed configuration passed all
33. Clang 18 ASan/UBSan passed all 31 applicable tests and built both libFuzzer
targets with diagnosis in the isolated fuzz core. The observation fuzzer
completed 100 mutation runs over a disposable corpus. Static and
diagnosis-specific runtime checks confirmed the offline boundary, and the CLI
exposes no observation or diagnosis surface. The pre-advance plan digest was
`sha256:728f36b8383fb4cc1216c2a2ca3bdea77f098122895b3cebc469d6608eb5b2d6`.
