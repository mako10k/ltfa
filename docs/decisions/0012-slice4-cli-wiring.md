# ADR 0012: Slice 4 CLI wiring

Status: Accepted

Date: 2026-08-19

Owner acceptance: 2026-08-24

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

## Claims and evidence

- **E-S4CLI-001:** The implementation checkpoint in
  `docs/acceptance/m4-cli-wiring.md` records Release 49/49, focused UBSan 1/1, installed execution,
  canonical observation-root rejection, structured result coverage, and zero-network tracing.
- **E-S4CLI-002:** Receipt `RCPT_MVP_OWNER_ACCEPTED_20260824` records final owner acceptance in
  commit `bf053b17f7a0ba85b241fa48a63b8ca2ec6bf8a1`.
- **E-S4CLI-003:** Commits `d18cafdf014acae4a930af5b4fb526c10bcef965` and
  `85faabf1129a7fb3dd44b3e451ba705395f6f6d1` complete the benchmark task and advance the plan to
  `MVP_ACCEPTED`; current readback reports no blocking criterion.
- **C-S4CLI-001 (high confidence):** The implemented `cuts` and `diagnose` commands satisfy the
  reviewed deterministic, bounded, local, and offline Slice 4 CLI boundary.
  - References: E-S4CLI-001, E-S4CLI-002, E-S4CLI-003

## Security and compatibility

Restricted IDs require `--include-restricted`. Model and observation inputs remain local and
bounded. No command execution, plugin loading, producer connection, database, network access, or
environment lookup is added. Existing `model` and `analyze` grammar and schemas are unchanged.

## Alternatives

- Keep cuts and diagnosis as source-tree C++ APIs only: rejected because the user explicitly
  requested installed CLI wiring before benchmark acceptance.
- Let `diagnose` obtain observations or time implicitly: rejected because it would violate the
  explicit local-input and no-clock boundary.
- Execute a recommended probe: rejected because recommendation and operational authorization are
  separate concerns.

## Consequences

The benchmark phase can measure the real CLI path. The installed artifact remains one executable
without an installed SDK.

## Implementation notes

- `app/cli.cpp` owns bounded command dispatch and local input handling.
- `app/cli_contract.cpp` owns public command discovery and status/schema metadata.
- `tests/cli_slice4_acceptance_test.cmake` and the offline runtime checks cover the accepted CLI
  boundary.
- **A-S4CLI-001 (implemented):** Wire `cuts` and `diagnose` under the bounded offline contract.
  - References: C-S4CLI-001
- **A-S4CLI-002 (implemented):** Reconcile the historical checkpoint with final MVP acceptance.
  - References: C-S4CLI-001

## Post-acceptance disposition

The implementation checkpoint intentionally left the exact command and schema contract pending on
2026-08-19. The final MVP acceptance subsequently incorporated the implemented `cuts` and
`diagnose` boundary: receipt `RCPT_MVP_OWNER_ACCEPTED_20260824` records the owner's acceptance in
commit `bf053b17f7a0ba85b241fa48a63b8ca2ec6bf8a1`, the benchmark task was completed in
`d18cafdf014acae4a930af5b4fb526c10bcef965`, and the accepted plan reached `MVP_ACCEPTED` in
`85faabf1129a7fb3dd44b3e451ba705395f6f6d1`.

The current `plans/ltfa-mvp.pert` readback reports `MVP_ACCEPTED` as both reached and accepted with
no blocking criterion. The implemented Slice 4 CLI contract is therefore accepted. Later CLI
Contracts 3 and 4 add discovery and guidance under ADRs 0014 and 0015 without changing the Slice 4
command effects, result schemas, or offline boundary.

## Review

The implementation and test boundary was reviewed at the 2026-08-19 checkpoint. Final owner
acceptance was recorded on 2026-08-24 as part of the optimized offline MVP acceptance. The accepted
scope does not include publication, active probing, an installed SDK, or implicit environment
access.

## Follow-ups

- Observation-format onboarding requires its own reviewed Guide contract under Issue #2.
- Installing the C++ core as an SDK remains a separate architecture and compatibility decision.
