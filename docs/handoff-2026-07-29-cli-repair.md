# LTFA CLI Repair Handoff — 2026-07-29

## Confirmed checkpoint

Branch: `main`

Issue: [#1 Restore assurance CLI consistency, symmetry, and coverage](https://github.com/mako10k/ltfa/issues/1)

Accepted implementation commits:

- `04577cc6308118b95f0f1a403a618ca244ee8318` repairs cumulative path/SPOF
  budgets and CLI acceptance coverage.
- `323367a0c0dea6ab67b705b761b06d81f0a664bd` defines the complete public
  `ltfa.assurance/v1` field inventory.

Acceptance records:

- `docs/acceptance/issue-1-cli-repair-wave1.md`
- `docs/acceptance/issue-1-assurance-field-inventory.md`

The Release configuration passes 23/23 CTest cases. The work remains one-shot,
read-only, and offline.

## Residual PERT

`plans/v0.2.1-cli-consistency.pert` is the authority for remaining Issue #1
work. It declares `CODEX=2` and `REVIEW=1`.

Completed and advanced:

- `CLI_SHARED_BUDGET_REPAIR`
- `CLI_HELP_AND_ACCEPTANCE_COVERAGE`
- `CLI_ASSURANCE_FIELD_INVENTORY`

Ready and recommended:

- `CLI_OUTPUT_PARITY` — expected 1 day

After output parity, `CLI_REPAIR_ACCEPTANCE` is the final remaining task.

## Resume procedure

```sh
git status --short --branch
perttool document check plans/v0.2.1-cli-consistency.pert --warnings-as-errors
perttool dag next plans/v0.2.1-cli-consistency.pert \
    --capacity CODEX=2 --capacity REVIEW=1
cmake --preset release
cmake --build build/release -j2
ctest --preset release --output-on-failure
```

Implement human-output parity from the field inventory in ADR 0003. Preserve
JSON schema v1, deterministic ordering, and restricted-ID behavior.

## Stop conditions and unexecuted work

Stop for a separate compatibility decision if parity work would add, remove,
or rename a JSON field. Stop if it would weaken restricted-ID handling or the
offline boundary.

The following are not complete or authorized by this handoff:

- human/JSON parity implementation
- final sanitizer and release-candidate acceptance
- Issue #1 closure
- version change, tag creation, or v0.2.1 publication

## Short retrospective

The review found a cross-phase defect that per-algorithm tests could not see.
Keeping a dedicated CLI cumulative-budget test now protects that integration
boundary. Defining the public field inventory before changing human output
also separates compatibility decisions from renderer implementation.
