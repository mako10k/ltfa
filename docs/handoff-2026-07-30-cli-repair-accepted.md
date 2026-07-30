# LTFA CLI Repair Accepted Handoff — 2026-07-30

## Confirmed checkpoint

Branch: `main`

Issue: [#1 Restore assurance CLI consistency, symmetry, and coverage](https://github.com/mako10k/ltfa/issues/1)

Final acceptance commit:
`b7f3ab83d6fb73d47cddaaaf853b6583856bba94`

Published repair baseline:
`d60d826b4352fb8d780bf8fee5833c23cc9805af`

The repair is accepted and published. `plans/v0.2.1-cli-consistency.pert` has
reached `CLI_REPAIR_ACCEPTED` and contains no remaining task or gate.

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

The repair baseline is published on the public `main` branch. GitHub Issue #1
was closed as completed on 2026-07-30 with a comment linking the acceptance
record and complete verification matrix.

The repository currently has no configured GitHub Actions workflow. GitHub
reported zero Actions runs, check-runs, and commit statuses for the published
repair baseline. There is therefore no CI failure, but also no CI success
signal; the acceptance authority is the recorded local verification matrix.

Preparing version `0.2.1`, creating a tag, and publishing a release remain
separate owner decisions. The executable still reports `ltfa 0.2.0`; no
`v0.2.1` tag or release exists from this acceptance work.

## Resume checks

```sh
git status --short --branch
perttool document check plans/v0.2.1-cli-consistency.pert --warnings-as-errors
perttool dag next plans/v0.2.1-cli-consistency.pert \
    --capacity CODEX=2 --capacity REVIEW=1
ctest --preset release
```

Expected PERT result: no recommended, runnable, blocked, or upcoming task.
