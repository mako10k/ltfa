# LTFA v0.2.0

Release date: not yet published

LTFA v0.2.0 is the first offline assurance candidate. It keeps the deterministic
local model compiler from v0.1.0 and adds explicit, bounded path, impact, SPOF,
and declared-failure analysis through the one-shot CLI.

## Included

- native C++17 core and one-shot `ltfa` CLI
- bounded DSL v1 lexer, parser, and model-root-safe include resolver
- typed model validation and deterministic normalization
- human summaries, `ltfa.model/v1` JSON, and DOT model output
- requirement-group availability and failure-domain semantics
- filtered reachability, representative paths, and modeled removal impact
- immediate and fixed-delay declared failure propagation
- articulation, bridge, and completeness-qualified SPOF analysis
- descriptor-backed CLI Contract 2 human and JSON help
- deterministic human and `ltfa.assurance/v1` analysis output
- explicit analysis scope, source, target, resource budgets, and restricted-ID
  acknowledgment
- stable usage, input, validation, and analysis-selection diagnostics
- MIT licensing and CMake install support

## Explicitly Not Included

This release does not collect observations, diagnose or infer a live failure
cause, calculate service-oriented cut sets, probe a target, execute a command,
integrate with monitoring, load plugins, use a database, run as a daemon, or
access the network.

## Build, Test, And Install

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release
cmake --install build/release --prefix /path/to/ltfa
```

The installed executable is `/path/to/ltfa/bin/ltfa`. License, README, release
notes, and the development guide are installed under
`/path/to/ltfa/share/doc/ltfa`. The install smoke test exercises installed
version and help output, model compilation, and one complete assurance
analysis.

Tag creation and release publication remain pending separate authorization.

## Public Contract

- CLI Contract: 2
- DSL: LTFA DSL v1
- model JSON: `ltfa.model/v1`
- assurance JSON: `ltfa.assurance/v1`
- command help JSON: `ltfa.command-help/v1`
- CLI error JSON: `ltfa.cli-error/v1`
- process statuses: 0 result, 2 usage, 3 input, 4 validation, 5 output

See `docs/decisions/0001-bootstrap-boundary.md`,
`docs/decisions/0002-cli-contract-and-first-release-boundary.md`, and
`docs/decisions/0003-v0.2-assurance-cli.md` for the accepted architecture and
release boundaries.
