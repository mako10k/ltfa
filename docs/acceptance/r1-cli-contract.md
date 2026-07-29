# R1 CLI Contract Implementation Evidence

Date: 2026-07-29

Implementation status: verified

Release status: not accepted or published

## Implemented Boundary

R1 implements the CLI contract accepted in Decision 0002 without changing the
Slice 1 model semantics, the project version, or the offline process boundary.

Delivered:

- one dependency-free immutable descriptor table for command parsing, text
  help, JSON help, usage validation, and acceptance tests
- equivalent `--help`, `-h`, `help`, and command-specific help routes
- byte-stable `ltfa.command-help/v1` discovery with command, operand, option,
  effect, schema, exit-status, and example metadata
- no-argument usage failure and stable `LTCLI-001` text diagnostics
- deterministic command and option suggestions with the most specific help
  route
- `--name value` and `--name=value` option spellings
- `ltfa.cli-error/v1` usage, input, and validation envelopes when JSON output
  is explicitly requested
- exact text/JSON standard-output, standard-error, and exit-status acceptance
  coverage

Preserved:

- successful JSON output remains `ltfa.model/v1`
- human and DOT failures leave standard output empty and retain stable `E_*`
  source diagnostics on standard error
- exit statuses remain 0 for success, 2 for usage, 3 for input/include/parse,
  4 for validation, and 5 for output failure
- the process remains one-shot and performs no network access, command
  execution, environment access, plugin loading, database access, or Graphviz
  invocation

## Verification

The following committed configurations passed from the same source state:

```text
Release:            15/15
ASan/UBSan:         13/13
LeakSanitizer:      13/13
ASan/UBSan + seeds: 14/14
```

The Release suite included static binary/source boundary checks, parser and
complete-CLI `strace` checks with zero network syscalls, and external Graphviz
validation. The fuzz-seed configuration replayed the committed parser corpus.

Focused CLI acceptance verifies:

- no arguments exit 2 with `LTCLI-001`
- root and model help aliases are equivalent
- text and JSON help are byte-stable
- structured help reports CLI Contract 1 and read-only effects
- unknown command and option suggestions are deterministic
- invalid enums, missing and duplicate required options, extra operands, and
  help conflicts fail with status 2
- JSON usage/input/validation errors retain status, stable code, source, and
  related-location information
- spaced and equals option spellings produce identical successful JSON

## Remaining Release Work

R1 did not itself authorize or complete a release. A separately authorized
release-preparation follow-up on 2026-07-29 completed:

- MIT license selection and recording
- the project version change to `0.1.0`
- release notes and an install-tree smoke test
- an accepted local release-preparation plan
- candidate verification across the committed CMake presets

The follow-up evidence is in
[`v0.1.0-release-candidate.md`](v0.1.0-release-candidate.md). The commit
containing that record is the immutable accepted candidate. Tag creation and
publication remain incomplete and still require separate authorization.

The residual offline-MVP PERT remains unchanged and still recommends
`M2_REQUIREMENT_SEMANTICS` for the independent Slice 2 workstream.
