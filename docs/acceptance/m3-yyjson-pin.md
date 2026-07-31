# M3 yyjson Pin Acceptance

Date: 2026-07-31

Task: `M3_PIN_YYJSON`

Status: accepted

## Pin evidence

The official GitHub release API reported `0.12.0` as the latest stable yyjson
release. It was published on 2025-08-18 and resolves to commit
`8b4a38dc994a110abaec8a400615567bd996105f`.

The exact vendored files and SHA-256 values are:

```text
45e384d3d52c73cba3a64d6e6c25d47cd738cd8a55c30629e3201046eda62947  third_party/yyjson/LICENSE
ac2e9bbb2e2d9149d90878d40506a1d624fa0b33c979a11b61075c54782c6d6a  third_party/yyjson/yyjson.c
175867c5493a5df648cec566717fa1c29aa2f6096f5f0cf1efad0b65e1f6d7b3  third_party/yyjson/yyjson.h
```

The license text identifies the MIT License and retains the upstream copyright.
The header reports `YYJSON_VERSION_STRING` as `0.12.0`.

## Build boundary

`ltfa_yyjson` is an isolated C99 static target. Its generated compile flags
contain the four accepted feature definitions and do not contain the project
warning flags. The C++ consumer test includes the vendor directory as a system
path and retains `-Wall -Wextra -Wpedantic -Werror`.

The target shares sanitizer instrumentation but is not linked into `ltfa_core`
or the CLI. No observation input, new command, schema change, file access path,
or runtime dependency is introduced by this task.

## Strict-reader evidence

`ltfa.third-party.yyjson` verifies:

- the compile-time version is exactly `0.12.0`
- one UTF-8 observation-shaped JSON object, including a Japanese string, parses
- the schema string is preserved
- trailing commas, NaN, comments, invalid UTF-8, and a second JSON document are
  rejected under `YYJSON_READ_NOFLAG`
- writer, incremental-reader, utility, and non-standard support are disabled

## Validation

```text
Release                       31/31 passed
ASan + UBSan                  28/28 passed
LeakSanitizer                 28/28 passed
ASan + UBSan fuzz-seed replay 29/29 passed
```

The Release suite includes installed-tree, static/runtime offline-boundary, and
all available Graphviz checks. The focused yyjson test passed in every
configuration, and the fuzz-seed configuration retained the existing parser
corpus replay.

PERT advancement remains a separate residual-plan mutation after this
acceptance and its implementation commit are preserved.
