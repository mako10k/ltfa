# LTFA v0.3.0-alpha.1

Release date: 2026-07-31

LTFA v0.3.0-alpha.1 is a prerelease checkpoint for the one-shot offline
analyzer. It adds smaller semantic topology views, explicit link aggregation
and protection-domain modeling, and the accepted source-tree observation and
diagnosis core while preserving the released CLI contract.

## Added To The CLI

- semantic `topology`, `logical`, `protection`, `power`, `dependency`, and
  `overlay` DOT views using schema `ltfa.diagram.dot/v1`
- deterministic containment clusters and owned-endpoint ports
- relation lenses that omit unrelated edge families without changing the
  lossless raw DOT default
- first-class point-to-point links, LAG/bond aggregates, and STP/ring control
  domains
- K-of-N aggregate availability using `mode=at-least minimum=N`
- explicit forwarding and standby design-state metadata in logical and
  protection diagrams

## Added To The Source Core

- pinned yyjson 0.12.0 with recorded MIT license and checksums
- strict bounded `ltfa.observation/v1` JSON Lines decoding
- explicit-reference-time freshness and conflict indexing
- bounded causal-cone single-failure hypothesis generation and deterministic
  evidence ranking
- versioned `ltfa.diagnosis/v1` JSON and `ltfa.diagnosis.dot/v1` rendering
- a synthetic structure-derived historical fixture with no operational values
  or live-site dependency

The observation, evidence, and diagnosis interfaces are source-tree C++ APIs.
This prerelease does not install a core SDK and does not expose observation
input or diagnosis through the `ltfa` CLI.

## Compatibility

- CLI Contract remains version 2.
- Existing `model` and `analyze` grammar and exit statuses are unchanged.
- LTFA DSL remains version 1 and adds only validated model vocabulary.
- Existing `ltfa.model/v1`, `ltfa.model.dot/v1`, and `ltfa.assurance/v1`
  contracts remain available.
- Raw DOT remains the default and byte-stable compatibility view.
- Restricted stable IDs still require explicit `--include-restricted`.
- Exit statuses remain 0 result, 2 usage, 3 input, 4 validation, and 5 output.

## Prerelease Boundary

This tag is a checkpoint, not the stable `v0.3.0` release. Bounded service cuts,
next-useful-observation recommendations, the Slice 4 acceptance gate, and MVP
benchmark acceptance remain planned work. Diagnosis CLI design and publication
as an installed API remain separate decisions.

## Safety Boundary

LTFA remains a one-shot, read-only, offline tool. It does not execute a probe,
connect to a production system, run a command from input, retrieve a secret,
load a plugin, use a database, or run as a daemon. Observation producers and
live-probe authorization remain outside this executable and release.

## Build, Test, And Install

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release
cmake --install build/release --prefix /path/to/ltfa
```

The installed executable is `/path/to/ltfa/bin/ltfa`. License, README, release
notes, and the development guide are installed under
`/path/to/ltfa/share/doc/ltfa`.

The `v0.3.0-alpha.1` tag identifies the immutable prerelease candidate recorded
in `docs/acceptance/v0.3.0-alpha.1-release-candidate.md`.

## Trace

- Semantic DOT decision: `docs/decisions/0004-semantic-dot-views.md`
- Link aggregation and protection decision:
  `docs/decisions/0005-link-aggregation-and-protection-domains.md`
- Observation and diagnosis decisions: `docs/decisions/0007-observation-json-lines-contract.md`
  through `docs/decisions/0011-diagnosis-machine-output.md`
- Slice 3 acceptance: `docs/acceptance/m3-observation-diagnosis-slice.md`
- Previous release notes: `docs/releases/v0.2.1.md`
