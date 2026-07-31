# v0.3.0-alpha.1 Release Metadata Acceptance

Date: 2026-07-31

Task: `R030A1_RELEASE_METADATA`

Status: accepted

## Accepted metadata

- CMake retains numeric project version `0.3.0` and adds the explicit
  prerelease component `alpha.1`; the public version string is
  `0.3.0-alpha.1` while `kVersion` remains the numeric tuple `0.3.0`.
- `ltfa --version`, structured help, CLI acceptance, install smoke, and the
  core version API all consume the same configured public string.
- `README.md` identifies this version as a prerelease checkpoint and keeps
  `v0.2.1` as the current stable release.
- `RELEASE_NOTES.md` distinguishes CLI additions from source-only observation
  and diagnosis APIs and explicitly records the missing diagnosis CLI and
  installed SDK.
- The tagged `v0.2.1` notes are preserved byte-for-byte at
  `docs/releases/v0.2.1.md`.
- The new release PERT keeps annotated tag publication after candidate
  acceptance; GitHub Release creation and stable-version promotion remain
  separate operations.

## Boundary

The prerelease metadata does not expose a new command, change CLI Contract 2,
publish a tag, create a GitHub Release, or claim Slice 4 and MVP benchmark work
as complete. Observation and diagnosis remain deterministic source-tree core
APIs. The installed product remains the offline `ltfa` executable and its
documentation.

## Focused evidence

The metadata implementation is accepted when the Release build reports
`ltfa 0.3.0-alpha.1`, the structured numeric version is `0.3.0`, version/help
and install-tree tests pass, the archived notes match the tagged `v0.2.1`
bytes, both active PERT plans validate, and neither a local nor remote
`v0.3.0-alpha.1` tag exists.

The complete Release, sanitizer, fuzz-seed, Clang, Graphviz, and offline matrix
belongs to the following candidate-acceptance task.

## Result

The Release executable reported:

```text
ltfa 0.3.0-alpha.1
```

`ltfa.smoke`, `ltfa.cli.version`, `ltfa.cli.acceptance`, and
`ltfa.install.smoke` passed 4/4. Both the residual MVP plan and this release
plan passed `perttool document check --warnings-as-errors`; `git diff --check`
passed; the historical notes matched `v0.2.1:RELEASE_NOTES.md`; and authenticated
remote readback plus the local ref check found no `v0.3.0-alpha.1` tag.
