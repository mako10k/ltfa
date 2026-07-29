# R020 v0.2 Assurance CLI Contract Evidence

Date: 2026-07-29

Task: `R020_CLI_CONTRACT`

Status: accepted

## Accepted Surface

ADR 0003 fixes one descriptor-backed `ltfa analyze` operation with:

- explicit model root, entry file, scope, source, and target
- one optional declared failure mode
- deterministic human and `ltfa.assurance/v1` JSON output
- explicit restricted-data acknowledgment
- positive path, SPOF, event, and state-transition work budgets
- stable help, usage errors, diagnostics, and exit statuses
- complete, incomplete, unreachable, and invalid-selection separation

## Convention Review

The contract was checked against the current local implementations rather than
older remembered output:

| Source | Convention retained |
| --- | --- |
| `perttool` | Root command catalog, exact descriptor-derived command help, operation/effect/schema/status disclosure, structured recovery hints. |
| `secexec` | Exact accepted grammar first, concise safety boundary, typed stable JSON errors, status `2` for usage failures. |
| Existing LTFA | `ltfa.command-help/v1`, `ltfa.cli-error/v1`, source-faithful compiler diagnostics, no-argument usage failure, explicit local model root. |

`secexec` remains intentionally more compact at its root because its execution
grammar is the central contract. `perttool` provides the stronger applicable
pattern for LTFA's growing command catalog.

## Review Findings

- Requiring `--scope` prevents an omitted topology region from being silently
  selected as the intended assurance boundary.
- `not-a-spof-in-view` remains explicitly view-limited.
- Unreachable is not rewritten as a SPOF absence claim.
- Incomplete work is a result, not a process error.
- Failure propagation accepts only a declared failure ID; the CLI does not
  infer cause.
- Restricted IDs require affirmative output authorization.
- Publication is not authorized by the CLI or release-preparation plan.

## Trace

- Architecture decision:
  `docs/decisions/0003-v0.2-assurance-cli.md`
- Release preparation:
  `plans/v0.2.0-release.pert`
- Pre-advance plan digest:
  `sha256:90b40cc5c0079a828b60c19fc1d16feec874f1d2a4add69d39cc7c1cf492acfa`
