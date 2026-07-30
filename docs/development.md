# Developing LTFA

This document contains repository-maintainer guidance. The root
[README](../README.md) is the user guide.

## Prerequisites

- CMake 3.20 or later
- a C++17 compiler
- Git
- `perttool` for plan maintenance
- `clang-format` for C++ formatting

Linux `strace` and Graphviz `dot` are optional. When present, the Release test
suite uses them for runtime network-syscall and DOT-render validation.
Clang is required only for libFuzzer builds.

## Setup and verification

Configure, build, and test the Release tree:

```sh
cmake --preset release
cmake --build --preset release
ctest --preset release
git diff --check
```

For a conventional non-preset build:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Compiler warnings are errors. GCC and Clang use
`-Wall -Wextra -Wpedantic -Werror`; MSVC uses `/W4 /WX`.

## Focused development

Exercise the built CLI directly:

```sh
./build/release/ltfa --help
./build/release/ltfa --version
./build/release/ltfa model \
    --model-root tests/fixtures/slice1 \
    --input main.ltfa \
    --format json
./build/release/ltfa analyze \
    --model-root tests/fixtures/slice2 \
    --input main.ltfa \
    --scope site:verified \
    --source device:path-source \
    --target device:path-tail \
    --failure failure:breaker \
    --format json
```

Tests are dependency-free executables registered with CTest. Name test sources
descriptively and CTest cases with the `ltfa.<area>[.<behavior>]` pattern. Add
each target to `tests/CMakeLists.txt`, and include focused success and failure
coverage for every behavior change.

## Sanitizers and fuzzing

Run the complete committed validation configurations with:

```sh
cmake --preset asan-ubsan
cmake --build --preset asan-ubsan
ctest --preset asan-ubsan

cmake --preset lsan
cmake --build --preset lsan
ctest --preset lsan

cmake --preset fuzz-seeds
cmake --build --preset fuzz-seeds
ctest --preset fuzz-seeds
```

The fuzz-seed preset replays the deterministic parser corpus under ASan/UBSan.
Clang libFuzzer commands and corpus rules are documented in the
[fuzzing guide](../tests/fuzz/README.md).

The suite statically rejects network, process-launch, plugin-loading, and
environment-access symbols. On Linux with `strace`, it traces the parser,
assurance core, and complete CLI. Graphviz validation renders emitted DOT to
SVG when `dot` is installed.

## Install-tree verification

`ltfa.install.smoke` owns only the build-local `install-smoke` prefix. It
recreates that prefix, installs the candidate, checks the executable and
distribution documents, and exercises installed version, help, model, and
assurance operations.

Do not use this test with a user or system installation prefix.

## Coding conventions

Format changed C++ files with the repository configuration:

```sh
clang-format -i path/to/changed.cpp path/to/changed.hpp
```

The style is LLVM-based with four-space indentation, 100-column lines, and
case-sensitive include sorting. Use `snake_case` for functions and local
variables, `PascalCase` for types, and `kPascalCase` for constants. Keep code in
the `ltfa` namespace, headers self-contained, and CMake settings target-scoped.

Public core interfaces belong in `include/ltfa/`; implementation details belong
in `src/ltfa/`. Repository-specific contribution, validation, Git, and task
selection rules are in [AGENTS.md](../AGENTS.md).

## Repository layout

```text
app/                 CLI entry point and output adapters
cmake/               Build and verification scripts
include/ltfa/        Public core headers
src/ltfa/            Core implementation
tests/               Dependency-free CTest targets
tests/fuzz/          Parser fuzz harness and synthetic seed corpus
tests/fixtures/      Synthetic offline acceptance inputs
docs/acceptance/     Requirement traces and accepted boundaries
docs/decisions/      Project-local architecture decisions
docs/plans/          Completed PERT task evidence
docs/releases/       Historical release notes
plans/               Residual perttool plans
third_party/         Approved pinned source dependencies
```

Treat `build/`, `build-*`, and `cmake-build-*` as generated output.

## Sources of truth

The reviewed product design currently remains in the adjacent operations
repository:

1. [Requirements](../../ops/docs/lightweight-topology-fault-analyzer-requirements.md)
2. [Basic design](../../ops/docs/lightweight-topology-fault-analyzer-basic-design.md)
3. [Design review](../../ops/docs/lightweight-topology-fault-analyzer-design-review.md)

Repository-local decisions define the implemented boundary:

- [Bootstrap boundary](decisions/0001-bootstrap-boundary.md)
- [CLI contract and first release boundary](decisions/0002-cli-contract-and-first-release-boundary.md)
- [v0.2 offline assurance CLI contract](decisions/0003-v0.2-assurance-cli.md)

Accepted implementation and release evidence is retained under
`docs/acceptance/` and `docs/plans/`. In particular:

- [integrated Slice 2 acceptance](acceptance/m2-offline-assurance-slice.md)
- [v0.2 CLI implementation](acceptance/r020-cli-implementation.md)
- [v0.2 release metadata](acceptance/r020-release-metadata.md)
- [v0.2 candidate acceptance](acceptance/v0.2.0-release-candidate.md)
- [v0.2 release history](plans/v0.2.0-release-history.md)
- [Issue #1 CLI repair wave 1](acceptance/issue-1-cli-repair-wave1.md)
- [Issue #1 assurance field inventory](acceptance/issue-1-assurance-field-inventory.md)
- [Issue #1 assurance output parity](acceptance/issue-1-assurance-output-parity.md)
- [Issue #1 CLI repair acceptance](acceptance/issue-1-cli-repair-acceptance.md)

## Plan maintenance

The post-v0.2.0 assurance CLI repair is tracked by
[GitHub Issue #1](https://github.com/mako10k/ltfa/issues/1) and the independent
`plans/v0.2.1-cli-consistency.pert` plan. Validate and select its repair
frontier with:

```sh
perttool document check plans/v0.2.1-cli-consistency.pert --warnings-as-errors
perttool dag next plans/v0.2.1-cli-consistency.pert \
    --capacity CODEX=2 --capacity REVIEW=1
```

The residual offline-MVP plan is `plans/ltfa-mvp.pert`. Validate it and select
the next capacity-feasible task with:

```sh
perttool document check plans/ltfa-mvp.pert --warnings-as-errors
perttool dag next plans/ltfa-mvp.pert --capacity CODEX=1 --capacity REVIEW=1
```

Use `perttool` mutation commands instead of editing existing `.pert` documents
directly. Preview each mutation, bind persistent writes to the current digest,
and re-read the result. Retain completed-task evidence in
`docs/plans/ltfa-mvp-history.md` before `perttool dag advance` removes it from
the residual graph.

The completed v0.2 candidate-preparation plan remains at
`plans/v0.2.0-release.pert`.

The current Issue #1 resume point is recorded in the
[2026-07-30 accepted CLI repair handoff](handoff-2026-07-30-cli-repair-accepted.md).

## Release work

Normal development does not create tags, push releases, or mutate remote
release state. The accepted v0.2 candidate and its verification matrix are
recorded in the
[candidate evidence](acceptance/v0.2.0-release-candidate.md).

Publication requires a separate owner instruction. The publication operation
must tag the immutable candidate commit identified by that evidence and verify
each requested remote reference after the write.
