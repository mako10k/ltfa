# R020 v0.2 Release Metadata Evidence

Date: 2026-07-29

Task: `R020_RELEASE_METADATA`

Status: accepted

## Prepared Metadata

- CMake project version, generated core version API, `--version`, and
  structured help now report `0.2.0`.
- `RELEASE_NOTES.md` describes the accepted offline assurance scope and its
  explicit exclusions without claiming publication.
- The accepted but unpublished v0.1.0 notes remain available at
  `docs/releases/v0.1.0.md`.
- README examples and status describe the installed `analyze` operation and
  keep tag creation and publication as a separate authorization boundary.

## Install Contract

The isolated install-smoke test recreates its build-local prefix and verifies:

- the installed `ltfa` executable
- installed `LICENSE`, `README.md`, and `RELEASE_NOTES.md`
- exact version output `ltfa 0.2.0`
- descriptor-backed root help
- installed offline `ltfa.model/v1` model compilation
- installed complete `ltfa.assurance/v1` analysis with a reachable path, three
  qualified SPOF findings, and four declared-failure events

## Verification

The configured Release build completed with warnings as errors and all 22 CTest
cases passed. The suite included the expanded install-tree check, CLI Contract
2 acceptance, static and runtime offline-boundary checks, and Graphviz
validation.

No tag or remote release reference was created or published.

## Trace

- Current notes: `RELEASE_NOTES.md`
- Historical notes: `docs/releases/v0.1.0.md`
- Release history: `docs/plans/v0.2.0-release-history.md`
- Pre-advance plan digest:
  `sha256:48a4ce9d8aa454e1d33d42db11fae6c1bb55a356917f817108d2e1dd0816be4b`
