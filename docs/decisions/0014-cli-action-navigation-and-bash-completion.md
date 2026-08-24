# Decision 0014: CLI Action Navigation And Bash Completion

Date: 2026-08-24

Status: accepted

## Context

Decision 0002 made one immutable command descriptor catalog authoritative for parsing, text and
JSON help, usage validation, and tests. The implemented CLI Contract 2 exposes deterministic help
targets and spelling suggestions, but a user still has to infer the next recovery step from an
error. The repository also does not install shell completion, so command, option, and enum-value
discovery is unavailable at the prompt.

The following local implementations were reviewed as design references only:

- `perttool` at `55e74db365c8f61aa8d0736703aea195d5529548`, whose structured command registry
  drives detailed help and whose diagnostics navigate to specific help or guide targets
- `secdat` at `8d41b1bce528cffb42119c6e33f43ef4adc30750`, whose installed bash script asks a
  private CLI completion endpoint for the current command and option surface

LTFA does not take a runtime or vendored dependency on either project.

## Decision

### CLI Contract 3

The additive discovery and recovery boundary is CLI Contract 3. Existing command names, options,
exit statuses, output routing, and the `ltfa.command-help/v1` and `ltfa.cli-error/v1` envelope
identities remain compatible.

Help adds deterministic navigation derived from the command catalog:

- root, command-specific, and JSON help routes
- the repository and installed bash-completion routes
- command-specific text and JSON navigation links

### Error action navigation

Every handled usage, input, model-validation, and analysis-selection error exposes two bounded next
actions:

1. a non-executing instruction to correct the reported problem and rerun the selected command;
   when a deterministic spelling suggestion exists, the instruction names the exact replacement
2. a read-only command that opens the most specific valid help target

Text diagnostics retain the compatible `help:` line and add a preceding `next:` line. Structured
errors add an ordered `actions` array with stable IDs, kinds, descriptions, and nullable
invocations. Navigation never applies a fix, retries an operation, reads a model to invent IDs, or
executes a probe or command.

### Descriptor-backed bash completion

The repository provides `completions/ltfa.bash` and installs it as
`${CMAKE_INSTALL_DATADIR}/bash-completion/completions/ltfa`. The wrapper asks
`ltfa __completion --bash ...` for candidates instead of carrying a second command table.

The private completion endpoint:

- derives visible commands, long and short options, and enum values from the immutable descriptors
- suppresses already-used non-repeatable options
- handles separated and `--option=value` enum completion
- returns explicit `plain`, `file`, `dir`, or `none` modes
- delegates local filename and directory expansion to bash
- does not load topology or observation files, enumerate model IDs, read the network, inspect the
  environment, or execute another program

`__completion` is a private transport, is absent from public help and suggestions, and accepts only
the exact `--bash` protocol marker. Other shells and semantic model-ID completion remain outside
this decision.

## Claims And Evidence

- **C-CLINAV-001:** The descriptor catalog can remain the single source of truth for help and shell
  candidates.
  - **E-CLINAV-001:** `app/cli_contract.cpp` already owns every visible command, option, enum, and
    help rendering path.
  - **E-CLINAV-002:** The reviewed `perttool` registry projects parsing and discovery from the same
    immutable descriptors.
- **C-CLINAV-002:** A private query endpoint plus a thin installed script avoids static completion
  drift without weakening the offline boundary.
  - **E-CLINAV-003:** The reviewed `secdat` completion script delegates candidate discovery to its
    CLI and keeps filesystem expansion in bash.
  - **E-CLINAV-004:** LTFA's boundary tests already reject network and process-launch symbols in the
    CLI and core.
- **C-CLINAV-003:** Recovery navigation can be useful without performing an automatic retry.
  - **E-CLINAV-005:** CLI Contract 2 already identifies a stable suggestion and the most specific
    help target; adding an explicit correction instruction does not require new authority.

## Alternatives

### Duplicate commands and options in the bash script

Rejected because every CLI addition would require a second manually synchronized table.

### Parse JSON help from bash

Rejected because that would add a JSON-tool dependency to interactive completion and increase
latency and quoting complexity.

### Complete topology IDs by loading the selected model

Rejected because completion must remain cheap and non-semantic, must not expose restricted IDs, and
must not blur discovery with model processing.

### Automatically run a suggested retry

Rejected because a diagnostic is evidence and navigation, not authorization to repeat an
operation with changed inputs.

## Consequences

- CLI Contract consumers can discover safe recovery actions without scraping prose.
- Bash completion stays synchronized with descriptor changes and remains dependency-free.
- User-visible diagnostic text changes additively; golden and JSON contract tests must cover the
  exact ordering.
- Completion of filesystem paths remains the shell's responsibility and is not proof that a path
  is valid below `--model-root`.

## Implementation Notes

- Extend `app/cli_contract.*` with navigation rendering and descriptor-backed completion queries.
- Keep the private protocol dispatch in `app/cli.cpp` before public command lookup.
- Add `completions/ltfa.bash`, install-tree verification, focused completion regression coverage,
  and CLI Contract 3 acceptance cases.
- Update README and development guidance without changing the one-shot offline core boundary.

Implementation actions trace as follows:

- **A-CLINAV-001:** Add text and JSON action navigation.
  - References: C-CLINAV-003
- **A-CLINAV-002:** Add and install descriptor-backed bash completion.
  - References: C-CLINAV-001, C-CLINAV-002
- **A-CLINAV-003:** Add contract, install, and offline-boundary regressions.
  - References: C-CLINAV-001, C-CLINAV-002, C-CLINAV-003

## Review

The user explicitly authorized strengthening help, error-time next-action navigation, and bash
completion using `perttool` and `secdat` as references. The decision preserves existing command
effects and the offline safety boundary, so its status is accepted.

Implementation and validation results are recorded in
[`../acceptance/cli-navigation-bash-completion.md`](../acceptance/cli-navigation-bash-completion.md).

## Follow-ups

- Fish, Zsh, and PowerShell completion require separate demand and contract review.
- Semantic ID completion requires a separate privacy, restricted-output, and performance decision.
