# LTFA

LTFA is the working name for a lightweight multilayer topology and fault
analyzer.

This repository is an early offline prototype. It contains a native C++17 core,
a bounded DSL v1 parser and model-root-safe include loader, deterministic
normalized-model compilation, human/JSON/DOT model output, a small CLI with
help and version output, requirement-group availability semantics,
failure-domain expansion, filtered reachability and path analysis,
connectivity-removal impact analysis, and dependency-free CTest coverage. The
core also runs deterministic immediate and fixed-delay failure propagation,
linear articulation and bridge analysis, and completeness-qualified SPOF
analysis. The CLI exposes those assurance results through one explicit,
bounded, offline analysis operation. LTFA performs no network access.

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
- [v0.2 offline assurance CLI contract](docs/decisions/0003-v0.2-assurance-cli.md)
- [Current release notes](RELEASE_NOTES.md)
- [Historical v0.1.0 release notes](docs/releases/v0.1.0.md)

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

The accepted but unpublished `v0.1.0` candidate is preserved as the first
offline model compiler preview. The local project version is now `0.2.0` for
the separately authorized assurance candidate-preparation path. Its functional
scope is the accepted `SLICE2_ACCEPTED` core plus the reviewed `analyze` CLI.
This version change does not authorize a tag or publication.

The detailed CLI and release contract is recorded in
[`docs/decisions/0002-cli-contract-and-first-release-boundary.md`](docs/decisions/0002-cli-contract-and-first-release-boundary.md).
Implementation evidence is recorded in
[`docs/acceptance/r1-cli-contract.md`](docs/acceptance/r1-cli-contract.md).
The first Slice 2 core evidence is recorded in
[`docs/acceptance/m2-requirement-semantics.md`](docs/acceptance/m2-requirement-semantics.md).
Filtered path and removal-impact evidence is recorded in
[`docs/acceptance/m2-path-impact.md`](docs/acceptance/m2-path-impact.md).
Deterministic failure-propagation evidence is recorded in
[`docs/acceptance/m2-failure-propagation.md`](docs/acceptance/m2-failure-propagation.md).
Qualified articulation, bridge, and SPOF evidence is recorded in
[`docs/acceptance/m2-spof-analysis.md`](docs/acceptance/m2-spof-analysis.md).
The integrated offline-assurance acceptance is recorded in
[`docs/acceptance/m2-offline-assurance-slice.md`](docs/acceptance/m2-offline-assurance-slice.md).
The v0.1 release plan and candidate evidence are recorded in
[`docs/plans/v0.1.0-release.md`](docs/plans/v0.1.0-release.md) and
[`docs/acceptance/v0.1.0-release-candidate.md`](docs/acceptance/v0.1.0-release-candidate.md).
Release publication remains outside the residual MVP PERT and requires a
separate authorization after candidate acceptance.

The separately authorized local `v0.2.0` candidate-preparation path is tracked
in `plans/v0.2.0-release.pert`. It excludes tag creation and publication.

## Current CLI

```bash
./build/ltfa --help
./build/ltfa help model
./build/ltfa help model --format json
./build/ltfa --version
./build/ltfa model --model-root tests/fixtures/slice1 --input main.ltfa --format json
./build/ltfa analyze \
    --model-root tests/fixtures/slice2 \
    --input main.ltfa \
    --scope site:verified \
    --source device:path-source \
    --target device:path-tail \
    --failure failure:breaker \
    --format json
```

The `model` command loads, validates, normalizes, and renders one explicitly
rooted local topology as human text, JSON, or DOT. The `analyze` command loads
the same local model and analyzes an explicit scope, source, and target. It
returns deterministic human or `ltfa.assurance/v1` JSON path and qualified SPOF
results, with optional propagation of one declared failure mode. Positive work
budgets bound path traversal, candidate checks, events, and state transitions.
Restricted model IDs require `--include-restricted`; neither command performs
observations, probes, command execution, or network activity.

Invoking `ltfa` without a command is a usage error with status 2.
Machine-readable help uses `ltfa.command-help/v1`; JSON-mode usage, input, and
validation failures use `ltfa.cli-error/v1`. An incomplete budget-bounded
analysis and an unreachable target remain structured successful results; an
invalid analysis selection uses status 4.

The offline assurance core for `v0.2.0` evaluates requirement groups and
failure domains. It also supports bounded filtered
reachability, deterministic representative paths, and modeled connectivity
impact for object, relation, and failure-domain removal. One object, relation,
failure-domain, or declared-mode injection can be propagated to a fixed point
with stable immediate and delayed findings. Undirected graph views expose
articulation points and bridges, while source-to-target removal checks
distinguish `confirmed-spof`, `model-spof`, and `not-a-spof-in-view`, including
shared failure-domain candidates. These capabilities have passed one
cross-feature offline acceptance fixture and are exposed through the reviewed
CLI contract without widening the offline boundary.

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
docs/releases/       Historical release notes
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
