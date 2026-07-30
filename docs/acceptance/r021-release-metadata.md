# v0.2.1 Release Metadata Acceptance

Date: 2026-07-30

Implementation commit: `e1d8f3cbb7eaf831bdb662e18197aaa79678902c`

## Accepted metadata

- `project(ltfa VERSION 0.2.1)` remains the single source for the core version
  API, `ltfa --version`, structured help, and install-smoke expectation.
- `README.md` identifies `v0.2.1` as the current patch release and points to
  the matching release notes and GitHub Release URL.
- `RELEASE_NOTES.md` describes only the accepted Issue #1 budget, help,
  coverage, and assurance-output repairs.
- The notes explicitly retain CLI Contract 2, DSL v1, every JSON schema,
  restricted-output behavior, exit statuses, and the one-shot offline boundary.
- The previous `v0.2.0` notes are preserved byte-for-byte at
  `docs/releases/v0.2.0.md`.

## Evidence

The implementation commit passed:

```text
cmake --preset release
cmake --build --preset release -j2
./build/release/ltfa --version
ctest --test-dir build/release --output-on-failure \
    -R 'ltfa.cli.version|ltfa.install.smoke|ltfa.cli.assurance-output-parity|ltfa.cli.analyze.budget-acceptance'
```

Observed version: `ltfa 0.2.1`

Focused result: 4/4 tests passed.

`perttool document check plans/v0.2.1-release.pert
--warnings-as-errors`, `git diff --check`, and a byte comparison between the
tagged `v0.2.0` release notes and their historical copy also passed.

No `v0.2.1` tag or GitHub Release was created by this metadata task.
