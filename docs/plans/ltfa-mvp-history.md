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
