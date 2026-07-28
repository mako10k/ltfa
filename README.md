# LTFA

LTFA is the working name for a lightweight multilayer topology and fault
analyzer.

This repository is an early offline prototype. It contains a native C++17 core,
a bounded DSL v1 parser and model-root-safe include loader, deterministic
normalized-model compilation, human/JSON/DOT model output, a small CLI with
help and version output, and dependency-free CTest coverage. It does not yet
run topology analysis or perform network access.

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

The include loader accepts an explicit model root and entry file. Included paths
must remain local and relative, and are canonicalized below that root. Defaults
limit nesting to 16, each source to 16 MiB, and the complete load to 64 MiB.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Equivalent committed presets are available with `cmake --preset release` and
`cmake --build --preset release`.

## Test

```bash
ctest --test-dir build --output-on-failure
```

Run the explicit sanitizer configurations with:

```bash
cmake --preset asan-ubsan
cmake --build --preset asan-ubsan
ctest --preset asan-ubsan

cmake --preset lsan
cmake --build --preset lsan
ctest --preset lsan
```

`cmake --preset fuzz-seeds` enables deterministic corpus replay. Clang
libFuzzer commands and corpus rules are documented in `tests/fuzz/README.md`.
The test suite statically rejects network, process-launch, plugin-loading, and
environment-access symbols. On Linux with `strace`, it also verifies that the
parser integration test makes no network syscall. Graphviz remains optional;
when `dot` is installed, CTest validates generated LTFA DOT by rendering SVG.

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
./build/ltfa model --model-root tests/fixtures/slice1 --input main.ltfa --format json
```

The `model` command loads, validates, normalizes, and renders one explicitly
rooted local topology as human text, JSON, or DOT. It performs no topology
analysis or network activity. Analysis operations will be added in reviewed
slices.

## Layout

```text
app/                 CLI entry point
cmake/               Generated-build configuration templates
include/ltfa/        Public core headers
src/ltfa/            Core implementation
tests/               Dependency-free CTest targets
tests/fuzz/          Parser fuzz harness and synthetic seed corpus
tests/fixtures/      Synthetic offline acceptance inputs
docs/acceptance/     Requirement traces and accepted slice boundaries
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
