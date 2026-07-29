# LTFA v0.1.0

Release date: not yet published

LTFA v0.1.0 is the first offline model compiler preview. It loads one explicitly
rooted LTFA DSL v1 model, resolves bounded local includes, validates and
normalizes the model, and renders deterministic human, JSON, or DOT output.

## Included

- native C++17 core and one-shot `ltfa` CLI
- bounded DSL v1 lexer, parser, and model-root-safe include resolver
- typed model validation and deterministic normalization
- human summaries, `ltfa.model/v1` JSON, and DOT output
- descriptor-backed human and `ltfa.command-help/v1` JSON help
- stable `LTCLI-001` usage diagnostics and `ltfa.cli-error/v1` JSON errors
- MIT licensing and CMake install support

## Explicitly Not Included

This release does not implement path search, impact analysis, articulation or
bridge detection, SPOF detection, failure propagation, observation diagnosis,
live probing, command execution, monitoring integration, plugins, databases,
or network access.

## Build, Test, And Install

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release
cmake --install build/release --prefix /path/to/ltfa
```

The installed executable is `/path/to/ltfa/bin/ltfa`. License, README, and
release notes are installed under `/path/to/ltfa/share/doc/ltfa`.

The candidate test matrix passed Release (16/16), ASan/UBSan (14/14),
LeakSanitizer (14/14), and ASan/UBSan with parser seed replay (15/15). Release
publication remains pending separate tag/publication authorization.

## Public Contract

- CLI Contract: 1
- DSL: LTFA DSL v1
- model JSON: `ltfa.model/v1`
- command help JSON: `ltfa.command-help/v1`
- CLI error JSON: `ltfa.cli-error/v1`
- process statuses: 0 success, 2 usage, 3 input, 4 validation, 5 output

See `docs/decisions/0001-bootstrap-boundary.md` and
`docs/decisions/0002-cli-contract-and-first-release-boundary.md` for the
accepted architecture and release boundaries.
