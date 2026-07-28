# Decision 0001: Bootstrap Boundary

Date: 2026-07-28

Status: accepted for scaffold

## Context

The reviewed LTFA design calls for a lightweight native analyzer with future
offline observation integration. The first project step must establish a
buildable and testable boundary without prematurely implementing the DSL,
analysis algorithms, or production-facing probes.

## Decision

- Use C++17 and target-scoped CMake configuration.
- Generate C++ version constants from the CMake project version so version
  metadata has one source.
- Build one internal static core library and one CLI.
- Use CTest with no test framework dependency for the first smoke test.
- Keep the normal process one-shot and single-threaded.
- Add no database, network code, plugin loader, or renderer dependency.
- Do not vendor yyjson until the reviewed observation-decoder slice.
- Emit DOT in a future slice; do not link Graphviz into the core.
- Keep active PING and heartbeat producers outside this repository boundary
  until separately designed and approved.

## Consequences

- The initial binary only exposes help and version information.
- The project can establish compiler, warning, build, and test behavior before
  domain code is introduced.
- Third-party license, version, and checksum evidence will be recorded when a
  dependency is first added.
- The final project name, license, and remote repository remain undecided.
