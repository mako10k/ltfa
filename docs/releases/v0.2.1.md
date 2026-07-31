# LTFA v0.2.1

Release date: 2026-07-30

LTFA v0.2.1 is a compatibility-preserving patch for the offline assurance CLI.
It repairs the cumulative work-budget contract and makes human assurance
output a lossless presentation of the existing `ltfa.assurance/v1` JSON
contract.

## Fixed

- enforce one cumulative object-work budget across path and SPOF analysis
- enforce one cumulative relation-work budget across path and SPOF analysis
- preserve deterministic successful incomplete results when either shared
  budget is exhausted
- render structural usage and explicit not-applicable state in human output
- render complete SPOF candidate, baseline path, retained path, qualification,
  and filtering facts in human output
- render propagation unresolved-mode, previous-state, event qualification,
  and unaffected-object facts in human output
- use stable `path-incomplete` and `target-unreachable` reason tokens
- show every accepted budget option in descriptor-backed `analyze` help
- cover structured `analyze` help, all budget options, selection failures,
  shared-budget boundaries, and human/JSON field parity with focused tests

## Compatibility

- CLI Contract remains version 2.
- LTFA DSL remains version 1.
- Model JSON remains `ltfa.model/v1`.
- Assurance JSON remains `ltfa.assurance/v1`; no field was added, removed, or
  renamed.
- Command help and CLI error schemas remain at version 1.
- Restricted stable IDs still require explicit `--include-restricted`.
- Exit statuses remain 0 result, 2 usage, 3 input, 4 validation, and 5 output.

## Safety Boundary

This release remains a one-shot, read-only, offline tool. It does not collect
observations, infer a live failure cause, probe a target, execute commands,
load plugins, use a database, run as a daemon, retrieve secrets, or access the
network.

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

The `v0.2.1` tag identifies the immutable candidate recorded in
`docs/acceptance/v0.2.1-release-candidate.md`.

## Trace

- Repair issue: [GitHub Issue #1](https://github.com/mako10k/ltfa/issues/1)
- Repair acceptance:
  `docs/acceptance/issue-1-cli-repair-acceptance.md`
- Assurance field inventory:
  `docs/decisions/0003-v0.2-assurance-cli.md`
- Previous release notes: `docs/releases/v0.2.0.md`
