# R020 v0.2 Assurance CLI Implementation Evidence

Date: 2026-07-29

Task: `R020_CLI_IMPLEMENTATION`

Status: accepted

## Delivered Surface

The descriptor-backed `ltfa analyze` operation now:

- loads one entry file below an explicit local model root
- requires an explicit scope, source, and target
- computes a bounded representative path and qualified source-to-target SPOFs
- optionally propagates one declared failure mode
- emits deterministic human or `ltfa.assurance/v1` JSON results
- retains complete, incomplete, unreachable, and invalid-selection outcomes
- requires `--include-restricted` before emitting restricted model IDs
- exposes positive path, candidate, event, and state-transition budgets

The root and detailed command help derive from the same descriptor table.
Usage failures retain `LTCLI-001` and status 2. Invalid selections use
`LTANL-001` and status 4. Bounded incompleteness and unreachable targets remain
successful structured results with status 0.

## Acceptance Coverage

The CLI acceptance test covers:

- exact text-help route parity for root, `model`, and `analyze`
- byte-stable structured help with CLI Contract 2
- byte-stable complete assurance JSON and deterministic human output
- verified and partial-view SPOF qualification
- an unreachable target without a false SPOF claim
- successful bounded incompleteness
- stable invalid-budget and invalid-selection diagnostics
- fail-closed restricted output and explicit inclusion
- four-event declared failure propagation including fixed-delay hold-up

The Linux runtime boundary traces both `model` and `analyze` and rejects any
network syscall. The complete Release build passed all 22 CTest cases with
warnings treated as errors.

## Boundary

The implementation only consumes the compiled offline model. It does not
collect observations, infer a failure cause, probe a target, execute a command,
load a plugin, or use the network. Tag creation and publication remain outside
this task.

## Trace

- Contract: `docs/decisions/0003-v0.2-assurance-cli.md`
- Contract acceptance: `docs/acceptance/r020-cli-contract.md`
- Release preparation: `plans/v0.2.0-release.pert`
- Pre-advance plan digest:
  `sha256:d8f6575741d81e6fc512a300a97731b20fe27505aa5c51c75b3b36d84cf8d0f0`
