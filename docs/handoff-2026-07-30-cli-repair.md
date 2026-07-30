# LTFA CLI Repair Handoff — 2026-07-30

## Confirmed checkpoint

Branch: `main`

Issue: [#1 Restore assurance CLI consistency, symmetry, and coverage](https://github.com/mako10k/ltfa/issues/1)

Completed locally:

- `2b99c97f674a88a0757499de0fd62e4e4bc1af3c` restores lossless human
  presentation of `ltfa.assurance/v1` and adds the dedicated parity regression.
- `74cc1e1f72c4689d9e11d363520bf6f7f9644432` accepts the output-parity task.
- `86f193d73c5e49fe0b6d3b9dc167aa8d640bf84a` advances the PERT document to
  the final acceptance task.

The repair series, including this handoff, has not been pushed. The accepted
implementation and residual plan remain local until a separate push request.

## Evidence

The Release configuration passes 24/24 CTest cases. The new
`ltfa.cli.assurance-output-parity` test:

- reconstructs human output from the complete public JSON field inventory
- covers complete, bounded-incomplete, and nullable results
- verifies byte stability for equivalent human and JSON invocations
- fails against the `v0.2.0` executable and passes after the repair

Installed-tree and static/runtime offline-boundary tests remain green.
Restricted analysis still requires explicit inclusion.

## Residual PERT

`plans/v0.2.1-cli-consistency.pert` now contains one task:

- `CLI_REPAIR_ACCEPTANCE` — expected 1.167 days, requiring `CODEX=1` and
  `REVIEW=1`

The task must run the complete release, sanitizer, installed-tree, offline,
deterministic complete/incomplete, and Issue-linked acceptance gates.

## Resume procedure

```sh
git status --short --branch
perttool document check plans/v0.2.1-cli-consistency.pert --warnings-as-errors
perttool dag next plans/v0.2.1-cli-consistency.pert \
    --capacity CODEX=2 --capacity REVIEW=1
cmake --preset release
cmake --build --preset release -j2
ctest --preset release
```

Start `CLI_REPAIR_ACCEPTANCE` before changing its acceptance record. Rebuild
and test the `asan-ubsan`, `lsan`, and `fuzz-seeds` presets from the exact
candidate tree.

## Stop conditions and unexecuted work

Stop and return to implementation if any verification configuration fails or
if the accepted JSON v1, restricted-output, or offline boundary changes.

The following are not complete or authorized by this handoff:

- final Issue #1 repair acceptance
- Issue #1 closure
- version change, tag creation, or v0.2.1 publication
- pushing the local repair series
