# Issue #1 Assurance Public Field Inventory

Date: 2026-07-29

Issue: [#1 Restore assurance CLI consistency, symmetry, and coverage](https://github.com/mako10k/ltfa/issues/1)

Implementation commit: `323367a0c0dea6ab67b705b761b06d81f0a664bd`

## Accepted contract

ADR 0003 now treats the JSON field paths of `ltfa.assurance/v1` as the
canonical machine-readable inventory and human output as a lossless
presentation of those facts.

The inventory covers:

- envelope, selected focus, restricted-output mode, and model inputs
- path status, qualification, representative evidence, and usage
- nullable structural analysis and all structural findings
- applicable and not-applicable SPOF variants, full path evidence, and usage
- optional propagation, event transitions and qualifications, affected-object
  classifications, and usage
- all five configured work bounds

The contract explicitly classifies normalized filters, replay requests,
propagation injection details, final snapshots, internal event causes, lost
probes, and failed relations as core-only data. Publishing any of those fields
requires a separately reviewed stable-ID, ordering, scope, redaction, and
compatibility decision.

No JSON field or schema version changed in this task. The next implementation
task is to make human output cover the accepted inventory without changing
JSON v1 or restricted-ID handling.

## Evidence

The implementation commit passed Release CTest 23/23,
`perttool document check plans/v0.2.1-cli-consistency.pert
--warnings-as-errors`, and `git diff --check`.
