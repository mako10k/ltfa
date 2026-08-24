# CLI Action Navigation And Bash Completion Evidence

Date: 2026-08-24

Implementation status: verified

Publication status: not requested or performed

## Implemented Boundary

This change implements CLI Contract 3 under Decision 0014 without changing command effects,
exit statuses, output schema identities, or the one-shot offline core boundary.

Delivered:

- descriptor-derived root and command help navigation for text and JSON consumers
- ordered `next:` and compatible `help:` lines on handled text errors
- ordered `correct-and-retry` and `inspect-help` actions in `ltfa.cli-error/v1`
- representative help examples for `cuts` and `diagnose`
- a private `ltfa __completion --bash ...` query endpoint backed by the command descriptors
- command, option, and enum completion, including `--option=value` and non-repeatable suppression
- shell-owned local file and directory expansion with space-preserving regression coverage
- installation at `${CMAKE_INSTALL_DATADIR}/bash-completion/completions/ltfa`

Preserved:

- diagnostics do not apply a correction or run a retry
- completion does not load models, invent or reveal semantic IDs, inspect the network, launch a
  process, or add a dependency on the reference projects
- the private completion transport is absent from public help and spelling suggestions
- Fish, Zsh, PowerShell, and semantic ID completion remain out of scope

## Reference Readback

The design comparison used immutable local source states:

- `perttool` at `55e74db365c8f61aa8d0736703aea195d5529548`
- `secdat` at `8d41b1bce528cffb42119c6e33f43ef4adc30750`

They are design references only; LTFA has no runtime or vendored dependency on them.

## Verification

The following configurations passed from the same implementation state with warnings as errors:

| Configuration | Result |
| --- | ---: |
| GCC Release | 51/51 |
| GCC ASan/UBSan | 40/40 |
| GCC LeakSanitizer | 40/40 |

The Release suite includes focused CLI action-navigation and completion cases, execution of the
installed completion backend, the bash-wrapper regression, install-tree verification, static and
runtime offline-boundary checks, the complete existing model and analysis suite, and external
Graphviz validation. Both sanitizer suites include the new CLI, wrapper, and install tests.

Focused acceptance verifies:

- text and JSON help expose deterministic root, command, and completion routes
- usage, input, validation, and selection errors expose bounded actions in stable order
- spelling recovery names the exact correction while retaining the most specific help command
- candidate sets come from public descriptors and exclude private transport names
- enum values work after both a separate option and an equals-form option
- previously used non-repeatable options are suppressed
- file and directory modes delegate expansion to bash and preserve spaces
- malformed private-protocol markers remain ordinary unknown public commands

`clang-format`, `git diff --check`, and the installed-tree smoke test also passed. No tag, release,
remote write, or dependency publication was performed.

## Claims, Evidence, And Actions

- **C-CLINAV-010 (high confidence):** CLI Contract 3 provides synchronized discovery and bounded
  recovery navigation while preserving the offline boundary.
  - **E-CLINAV-010:** The GCC Release suite passed 51/51, including static and runtime offline
    checks and the installed completion backend.
  - **E-CLINAV-011:** GCC ASan/UBSan passed 40/40.
  - **E-CLINAV-012:** GCC LeakSanitizer passed 40/40.
  - **E-CLINAV-013:** Focused CLI acceptance covers descriptor synchronization, action ordering,
    path modes, enum forms, and the private/public boundary.
- **A-CLINAV-001 (implemented):** Add text and JSON action navigation.
  - References: C-CLINAV-003, C-CLINAV-010
- **A-CLINAV-002 (implemented):** Add and install descriptor-backed bash completion.
  - References: C-CLINAV-001, C-CLINAV-002, C-CLINAV-010
- **A-CLINAV-003 (implemented):** Add contract, install, wrapper, and offline-boundary regressions.
  - References: C-CLINAV-001, C-CLINAV-002, C-CLINAV-003, C-CLINAV-010
