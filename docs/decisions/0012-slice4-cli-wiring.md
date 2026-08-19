# ADR 0012: Slice 4 CLI wiring

Status: Implemented; explicit owner acceptance pending

Date: 2026-08-19

## Context

Slice 4 initially exposed bounded cuts and next-observation recommendations only as source-tree
C++ APIs. The user requested CLI wiring before benchmark target selection. CLI Contract 2 already
defines stable discovery, error envelopes, local-root input, deterministic output, and exit status
classes.

## Implemented decision

- Add `ltfa cuts` for scoped object, relation, and failure-domain cut analysis.
- Add `ltfa diagnose` for bounded observation decoding, evidence indexing, diagnosis, and
  next-useful-observation recommendations.
- Require an explicit RFC3339 `--analysis-at`; never consult a system clock.
- Require the observation file to be relative to and canonically below `--model-root`.
- Preserve exit statuses 0 result, 2 usage, 3 input, 4 validation, and 5 output.
- Keep bounded-incomplete results at exit 0 and encode the status in the result.
- Retain CLI Contract 2. Add result schemas `ltfa.service-cuts/v1` and
  `ltfa.diagnosis-cli/v1`; preserve `ltfa.diagnosis.dot/v1` for DOT output.
- Keep all commands one-shot, read-only, deterministic, and offline. A recommendation returns a
  registered probe ID but never executes it.

## Security and compatibility

Restricted IDs require `--include-restricted`. Model and observation inputs remain local and
bounded. No command execution, plugin loading, producer connection, database, network access, or
environment lookup is added. Existing `model` and `analyze` grammar and schemas are unchanged.

## Consequences

The benchmark phase can measure the real CLI path. The installed artifact remains one executable
without an installed SDK. This record must not be described as an accepted ADR until the owner
explicitly accepts the exact command and schema contract.
