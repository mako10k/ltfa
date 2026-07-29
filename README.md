# LTFA

LTFA is the working name for a lightweight multilayer topology and fault
analyzer.

This repository is an early offline prototype. It contains a native C++17 core,
a bounded DSL v1 parser and model-root-safe include loader, deterministic
normalized-model compilation, human/JSON/DOT model output, a small CLI with
help and version output, requirement-group availability semantics,
failure-domain expansion, filtered reachability and path analysis,
connectivity-removal impact analysis, and dependency-free CTest coverage. The
CLI does not yet expose assurance analysis, and the core does not yet run SPOF
or failure-propagation analysis. LTFA performs no network access.

## Design source

The reviewed design currently lives in the adjacent operations repository:

- [Requirements](../ops/docs/lightweight-topology-fault-analyzer-requirements.md)
- [Basic design](../ops/docs/lightweight-topology-fault-analyzer-basic-design.md)
- [Design review](../ops/docs/lightweight-topology-fault-analyzer-design-review.md)

The implementation remains subject to those documents until they are
deliberately migrated into this repository.

Repository-local decisions:

- [Bootstrap boundary](docs/decisions/0001-bootstrap-boundary.md)
- [CLI contract and first release boundary](docs/decisions/0002-cli-contract-and-first-release-boundary.md)
- [v0.1.0 release notes](RELEASE_NOTES.md)

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

## Install

Install the one-shot CLI and its license and release documents into an explicit
prefix:

```bash
cmake --install build/release --prefix /path/to/ltfa
/path/to/ltfa/bin/ltfa --version
```

The executable is installed under `bin`; `LICENSE`, `README.md`, and
`RELEASE_NOTES.md` are installed under `share/doc/ltfa`.

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

## Release direction

The accepted `v0.1.0` release candidate is an offline model compiler preview
containing Slice 1 plus descriptor-backed help, stable CLI usage diagnostics,
MIT licensing, release notes, and install-tree validation. The commit
containing the candidate evidence is its immutable candidate; it has not been
tagged or published. Integrated path, impact, SPOF, and failure-propagation
analysis remains the later `v0.2.0` boundary at `SLICE2_ACCEPTED`.

The detailed CLI and release contract is recorded in
[`docs/decisions/0002-cli-contract-and-first-release-boundary.md`](docs/decisions/0002-cli-contract-and-first-release-boundary.md).
Implementation evidence is recorded in
[`docs/acceptance/r1-cli-contract.md`](docs/acceptance/r1-cli-contract.md).
The first Slice 2 core evidence is recorded in
[`docs/acceptance/m2-requirement-semantics.md`](docs/acceptance/m2-requirement-semantics.md).
Filtered path and removal-impact evidence is recorded in
[`docs/acceptance/m2-path-impact.md`](docs/acceptance/m2-path-impact.md).
The release plan and current candidate evidence are recorded in
[`docs/plans/v0.1.0-release.md`](docs/plans/v0.1.0-release.md) and
[`docs/acceptance/v0.1.0-release-candidate.md`](docs/acceptance/v0.1.0-release-candidate.md).
Release publication remains outside the residual MVP PERT and requires a
separate authorization after candidate acceptance.

## Current CLI

```bash
./build/ltfa --help
./build/ltfa help model
./build/ltfa help model --format json
./build/ltfa --version
./build/ltfa model --model-root tests/fixtures/slice1 --input main.ltfa --format json
```

The `model` command loads, validates, normalizes, and renders one explicitly
rooted local topology as human text, JSON, or DOT. It performs no topology
analysis or network activity. Analysis operations will be added in reviewed
slices. Invoking `ltfa` without a command is a usage error with status 2.
Machine-readable help uses `ltfa.command-help/v1`; JSON-mode usage, input, and
validation failures use `ltfa.cli-error/v1`.

Development after the accepted `v0.1.0` candidate is building the offline
assurance core for `v0.2.0`. Requirement groups and failure domains can now be
evaluated through the core API. The core also supports bounded filtered
reachability, deterministic representative paths, and modeled connectivity
impact for object, relation, and failure-domain removal. CLI exposure remains
deferred until SPOF and propagation tasks are integrated.

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
- License: [MIT](LICENSE)
