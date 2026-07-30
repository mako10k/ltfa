# LTFA CLI Repair Accepted Handoff — 2026-07-30

## Confirmed checkpoint

Branch: `main`

Issue: [#1 Restore assurance CLI consistency, symmetry, and coverage](https://github.com/mako10k/ltfa/issues/1)

Final acceptance commit:
`b7f3ab83d6fb73d47cddaaaf853b6583856bba94`

The repair is accepted locally. `plans/v0.2.1-cli-consistency.pert` has reached
`CLI_REPAIR_ACCEPTED` and contains no remaining task or gate.

## Verification evidence

The exact accepted candidate passed:

| Configuration | Result |
| --- | ---: |
| Release | 24/24 |
| ASan/UBSan | 21/21 |
| LeakSanitizer | 21/21 |
| ASan/UBSan plus standalone fuzz seeds | 22/22 |

The matrix includes the focused budget and assurance-output regressions,
installed-tree execution, static and runtime offline checks, and parser seed
replay. The parity regression fails against `v0.2.0` and passes after the
repair.

Detailed evidence is retained in
`docs/acceptance/issue-1-cli-repair-acceptance.md`.

## Publication state

The repair series remains local and has not been pushed. Issue #1 remains open.
The following actions require separate owner direction:

- push the local repair series
- comment on or close Issue #1
- prepare version `0.2.1`
- create a tag or publish a release

The executable therefore still reports `ltfa 0.2.0`; no `v0.2.1` tag or
release exists from this acceptance work.

## Resume checks

```sh
git status --short --branch
perttool document check plans/v0.2.1-cli-consistency.pert --warnings-as-errors
perttool dag next plans/v0.2.1-cli-consistency.pert \
    --capacity CODEX=2 --capacity REVIEW=1
ctest --preset release
```

Expected PERT result: no recommended, runnable, blocked, or upcoming task.
