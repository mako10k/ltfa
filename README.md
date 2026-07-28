# LTFA

LTFA is the working name for a lightweight multilayer topology and fault
analyzer.

This repository is an early offline prototype. It contains a native C++17 core,
a bounded DSL v1 lexer and statement parser, a small CLI with help and version
output, and dependency-free CTest coverage. It does not yet resolve includes,
compile a validated topology model, run analysis, or perform network access.

## Design source

The reviewed design currently lives in the adjacent operations repository:

- [Requirements](../ops/docs/lightweight-topology-fault-analyzer-requirements.md)
- [Basic design](../ops/docs/lightweight-topology-fault-analyzer-basic-design.md)
- [Design review](../ops/docs/lightweight-topology-fault-analyzer-design-review.md)

The implementation remains subject to those documents until they are
deliberately migrated into this repository.

## Boundary

The core is intended to:

- read a strict graph-oriented topology definition
- validate a typed multilayer model
- analyze paths, impact, SPOFs, failure modes, and offline observations
- emit deterministic text, JSON, and DOT

The core must not:

- run PING or custom heartbeat requests
- connect to production systems
- load binary monitoring plugins
- require a daemon, graph database, or runtime language interpreter
- execute commands supplied by topology or observation input

Future observation producers remain separate processes connected through a
versioned JSON Lines contract.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Test

```bash
ctest --test-dir build --output-on-failure
```

## Plan

The residual offline-MVP plan is maintained in `plans/ltfa-mvp.pert`. Validate
and select the next capacity-feasible task with:

```bash
perttool document check plans/ltfa-mvp.pert --warnings-as-errors
perttool dag next plans/ltfa-mvp.pert --capacity CODEX=1 --capacity REVIEW=1
```

Use `perttool` mutation commands rather than editing the DSL directly.
Completed-task evidence is retained in `docs/plans/ltfa-mvp-history.md` before
`perttool dag advance` removes it from the residual graph.

## Current CLI

```bash
./build/ltfa --help
./build/ltfa --version
```

Only `--help` and `--version` exist in the scaffold. Analysis operations will
be added in reviewed slices.

## Layout

```text
app/                 CLI entry point
cmake/               Generated-build configuration templates
include/ltfa/        Public core headers
src/ltfa/            Core implementation
tests/               Dependency-free CTest targets
docs/decisions/      Project-local architecture decisions
docs/plans/          Completed PERT task evidence
plans/               Residual perttool plans
third_party/         Pinned source dependencies when approved
```

## Status

- Language: C++17
- Build: CMake
- Tests: CTest
- Runtime dependencies: none beyond the C++ runtime
- Network behavior: none
- License: not yet selected
