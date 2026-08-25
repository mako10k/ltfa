# CI Baseline Evidence

Date: 2026-08-25

Task: `ESTABLISH_CI_BASELINE`

Status: local and GitHub-hosted validation passed; PERT acceptance declaration pending

## Claims

- `C-CI-001` (high confidence) — The committed workflow candidate expresses the supported GCC and
  Clang Release, ASan/UBSan, LSan, deterministic fuzz-seed, install-smoke, Graphviz, and offline
  validation matrix through existing CMake presets.
- `C-CI-002` (high confidence) — The workflow candidate is least privilege: it grants only
  `contents: read`, reads no repository secret, avoids `pull_request_target`, and disables persisted
  checkout credentials.
- `C-CI-003` (high confidence) — The pushed candidate completed every GitHub-hosted matrix job
  successfully at the same commit verified on `origin/main`.

## Evidence

- `E-CI-001` — `.github/workflows/ci.yml` uses the explicit `ubuntu-24.04` image and a five-entry
  compiler/preset matrix. Every entry runs configure, build, and CTest through the selected preset.
- `E-CI-002` — `tests/ci_contract_test.cmake` fails if triggers, the explicit runner, read-only
  permissions, the immutable checkout pin, credential containment, matrix coverage, or validation
  commands are removed, and rejects `pull_request_target` and `secrets.*`.
- `E-CI-003` — `actions/checkout` tag `v7.0.1` resolved to
  `3d3c42e5aac5ba805825da76410c181273ba90b1` at implementation time. The workflow records both the
  immutable SHA and the human-readable version comment.
- `E-CI-004` — GitHub's Ubuntu 24.04 runner inventory lists CMake, GCC, Clang, and Make. The workflow
  installs Graphviz and `strace` as validation-only tools so optional DOT and runtime network-syscall
  tests are present rather than silently skipped.
- `E-CI-005` — The first Clang Release build rejected an unused lambda capture in
  `app/cli_contract.cpp` under `-Werror`. Removing only that unused capture preserved behavior; the
  repeated GCC and Clang Release suites then passed in full.
- `E-CI-006` — GitHub Actions run
  [32799735043](https://github.com/mako10k/ltfa/actions/runs/32799735043) completed with conclusion
  `success` for head `b74ee0173649358b9fccc23e42f161753c618df5`. Its five jobs —
  `gcc-release`, `clang-release`, `gcc-asan-ubsan`, `gcc-lsan`, and `clang-fuzz-seeds` — each
  completed with conclusion `success`.
- `E-CI-007` — After the run completed, local `HEAD`, `origin/main`, and remote branch readback all
  resolved to `b74ee0173649358b9fccc23e42f161753c618df5`.

## Reasoning

Existing presets already define the project validation boundary. Reusing them prevents a second CI
build contract from drifting away from local development. A full commit SHA makes the only external
action reference immutable, while read-only token permissions and non-persisted credentials bound
the effect of untrusted pull-request code. The authorized push and exact-SHA readback confirm that
the same candidate passed both local and hosted validation.

## Actions

- `A-CI-001` [executed] — Add the least-privilege pinned workflow and its static contract test.
- `A-CI-002` [executed] — Run the local matrix and record exact results below.
- `A-CI-003` [executed] — Push the immutable candidate once and read back every hosted matrix result.
- `A-CI-004` [pending] — Obtain owner acceptance of the exact Grammar 7 milestone criteria before
  finishing and advancing `ESTABLISH_CI_BASELINE`.

## Local validation results

| Configuration | Compiler | Result |
| --- | --- | ---: |
| Release | GCC 13.3.0 | 53/53 passed |
| Release | Clang 18.1.3 | 53/53 passed |
| ASan/UBSan | GCC 13.3.0 | 42/42 passed |
| LSan | GCC 13.3.0 | 42/42 passed |
| standalone fuzz seeds under ASan/UBSan | Clang 18.1.3 | 44/44 passed |

Both Release runs included install-smoke, the static offline boundary, eleven runtime `strace`
checks, and eight Graphviz render checks. The workflow YAML syntax check, the new `ltfa.ci.contract`
test, and `git diff --check` also passed.

## GitHub-hosted validation results

| Job | Result | Duration |
| --- | --- | ---: |
| `gcc-release` | success | 58 seconds |
| `clang-release` | success | 61 seconds |
| `gcc-lsan` | success | 77 seconds |
| `clang-fuzz-seeds` | success | 126 seconds |
| `gcc-asan-ubsan` | success | 191 seconds |

Run `32799735043` was triggered by the authorized push event at
`b74ee0173649358b9fccc23e42f161753c618df5`; it completed with overall conclusion `success`.

## Boundary

The one authorized push has completed. This evidence does not authorize another push, change branch
protection, use secrets, contact a product endpoint, publish a tag or GitHub Release, or promote a
stable version. `apt-get` is confined to installing validation tools on an ephemeral CI runner and
is never called by LTFA.
