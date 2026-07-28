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
