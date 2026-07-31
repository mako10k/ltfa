# ADR 0006: Pin yyjson For Observation Decoding

Date: 2026-07-31

Status: Accepted

## Context

The next residual MVP task is the strict observation JSON Lines decoder.
Requirements permit one small JSON implementation, and the reviewed basic
design selects yyjson. The design review rejects a separately upgraded runtime
ABI and requires an exact source pin, license verification, checksum evidence,
and isolated third-party warnings before decoder implementation.

The decoder must reject JSON5 extensions, comments, trailing commas, invalid
UTF-8, infinity, NaN, and extra content after one record. LTFA, rather than the
JSON library, owns bounded file and standard-input handling.

## Decision

LTFA vendors yyjson release `0.12.0` at commit
`8b4a38dc994a110abaec8a400615567bd996105f`. The selected release was the
latest non-draft, non-prerelease GitHub release when checked on 2026-07-31.

Only the upstream amalgamated reader source and its license are copied:

- `third_party/yyjson/yyjson.c`
- `third_party/yyjson/yyjson.h`
- `third_party/yyjson/LICENSE`

`third_party/README.md` records commit-addressed source URLs and SHA-256 for
every copied file. The upstream license is MIT.

The isolated `ltfa_yyjson` C99 target defines:

- `YYJSON_DISABLE_WRITER=1`
- `YYJSON_DISABLE_INCR_READER=1`
- `YYJSON_DISABLE_UTILS=1`
- `YYJSON_DISABLE_NON_STANDARD=1`

UTF-8 validation remains enabled. Standard `YYJSON_READ_NOFLAG` behavior is the
decoder baseline. Fast numeric conversion remains enabled. File-reading helpers
present in the upstream 0.12.0 amalgamation are not an LTFA input boundary;
the future decoder must pass an already bounded in-memory record.

Project source retains `-Wall -Wextra -Wpedantic -Werror` or `/W4 /WX`.
Third-party C does not inherit those warning flags. Its public include
directory is marked `SYSTEM`, while its consumer test retains project warnings
as errors. The vendored target does inherit the configured sanitizer interface.

This pin task builds and directly tests yyjson but does not link it into
`ltfa_core`, change CLI or output schemas, decode observations, or add network,
command, plugin, database, or daemon behavior.

## Consequences

The observation decoder can be implemented against a reproducible source and
compile contract without a dynamic-library ABI dependency. A dependency update
requires a new reviewed commit, refreshed checksums and license evidence, and a
complete validation run.

The project now needs a C99 compiler in addition to its C++17 compiler.
Redistributed source retains the upstream license beside the vendored files.
