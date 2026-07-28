# Decision 0002: CLI Contract And First Release Boundary

Date: 2026-07-28

Status: accepted

## Context

Slice 1 is accepted as a one-shot offline model compiler. The current CLI can
load, validate, normalize, and render one explicitly rooted model, but its
help and usage-error behavior is still a bootstrap interface. In particular,
an invocation without a command currently prints help and exits successfully,
unknown commands have no stable CLI diagnostic code or suggestion, and the
help surface has no machine-readable contract.

The local latest implementations used as style references were:

- `secexec` at `91244453fe5d865168886cb5a2fabad8c57d0a61`
  (`0.3.0-dev`), whose top-level help is a concise invocation-first synopsis,
  whose no-argument invocation is usage failure, and whose usage failures exit
  with status 2
- `perttool` at `a81697c139a94fac11999a2e400aa802af82f5da`
  (`0.4.0`, CLI Contract 5), whose immutable command descriptors drive
  command-specific text and JSON help, exit-status discovery, deterministic
  usage suggestions, and structured CLI errors

These are design references only. LTFA does not depend on either program and
does not add a runtime or vendored dependency for its CLI.

## Decision

### First release boundary

The first release candidate is `v0.1.0`, described as an offline model
compiler preview. It contains the accepted Slice 1 model-loading,
normalization, validation, and human/JSON/DOT rendering boundary plus the CLI
contract and release preparation defined here.

`v0.1.0` does not claim path, impact, articulation, bridge, SPOF, failure
propagation, observation diagnosis, live probing, or production integration.
The next functional release boundary is `v0.2.0` at `SLICE2_ACCEPTED`.

Before a public `v0.1.0` publication:

- select and record the project license
- set the project version from `0.0.0` to `0.1.0`
- add release notes and an install-tree smoke test
- pass the committed Release, sanitizer, fuzz-seed, offline-boundary, CLI, and
  optional Graphviz checks applicable to the release environment
- record release acceptance before creating or publishing a tag

The residual MVP PERT explicitly excludes release publication. Release
preparation and publication therefore require a separate owner-approved plan;
the existing PERT is not edited directly to imply that authority.

### Human help

The top-level help remains concise and invocation-first, following the
`secexec` presentation style. Detailed help follows the descriptor-backed
`perttool` contract style.

The supported discovery routes are:

```text
ltfa --help
ltfa -h
ltfa help
ltfa help model
ltfa help model --format json
ltfa model --help
ltfa model -h
ltfa --version
```

`--help`, `-h`, valid `help` invocations, and `--version` write to standard
output and exit with status 0. An invocation without a command is CLI misuse:
it writes a concise diagnostic to standard error and exits with status 2.

Top-level text help lists the available commands and the offline safety
boundary. Command help describes:

- operation and summary
- operands and options
- required, repeatable, default, enum, conflict, and dependency semantics
- input and output formats
- process effect, including that `model` is a read-only offline operation
- result schemas and exit statuses
- representative examples

### Descriptor-backed discovery

One immutable command-descriptor table is the source of truth for parsing,
human help, JSON help, usage validation, and tests. The C++ implementation is
dependency-free and does not duplicate independent prose and parser tables.

Structured help uses the versioned schema `ltfa.command-help/v1`. It includes
the CLI contract version, tool version, query, command path, operation,
summary, operands, options, input, output, effect, result schemas, exit
statuses, examples, and diagnostics.

### Usage and diagnostic errors

CLI misuse uses stable code `LTCLI-001`, status 2, and a deterministic
classification such as missing command, unknown command, unknown option,
missing value, duplicate option, extra operand, missing required option, or
option conflict.

Text diagnostics use this shape:

```text
LTCLI-001 error: unknown command modle
  token: modle
  suggestion: model
  help: ltfa help
```

Suggestions must be deterministic members of the active command or option
scope. Every usage error identifies the most specific valid help target.

Human and DOT invocations write errors to standard error and leave standard
output empty. When a valid `--format json` or `--format=json` request can be
identified, usage, input, and validation failures write a versioned
`ltfa.cli-error/v1` envelope to standard output. That envelope retains the
underlying stable core diagnostic code, source locations, related location,
operation, usage classification when applicable, token, suggestion, and help
target. An output-stream failure is necessarily reported best-effort on
standard error.

The accepted process exit-status meanings remain:

| Status | Meaning |
| --- | --- |
| `0` | Successful operation or help/version request |
| `2` | CLI usage error |
| `3` | Input, include, or parse failure |
| `4` | Model validation failure |
| `5` | Output failure |

Core diagnostics retain their existing stable families such as `E_DSL_*`,
`E_INCLUDE_*`, `E_ID_*`, and `E_DEPENDENCY_*`.

### Verification contract

CLI acceptance tests cover:

- no arguments, root help, version, help aliases, and command help
- exact standard-output, standard-error, and exit-status routing
- unknown command and option suggestions
- required, duplicate, conflicting, and malformed options
- byte-stable human and JSON help
- the `ltfa.command-help/v1` and `ltfa.cli-error/v1` schemas
- preservation of existing input and validation diagnostics
- zero network, command execution, plugin loading, or environment access

## Consequences

- The help and error surfaces become versioned public interfaces for humans and
  automation.
- A new command or option must update the descriptor and its contract tests in
  the same change.
- The first implementation task is a release-preparation slice such as
  `R1_CLI_CONTRACT`; accepting this decision does not itself change the binary
  version, create a release tag, or authorize publication.
- Slice 2 implementation can proceed independently after the first-release
  decision is either completed or explicitly deferred.
