# Issue #1 Assurance Output Parity

Date: 2026-07-30

Issue: [#1 Restore assurance CLI consistency, symmetry, and coverage](https://github.com/mako10k/ltfa/issues/1)

Implementation commit: `2b99c97f674a88a0757499de0fd62e4e4bc1af3c`

## Accepted behavior

Human `analyze` output is now a lossless, deterministic presentation of the
accepted `ltfa.assurance/v1` public field inventory:

- structural analysis retains its usage and an explicit `not-applicable`
  representation
- SPOF findings expose separate candidate kind and ID, complete baseline and
  retained path evidence, qualification, and filter qualification
- not-applicable SPOF results use the stable `path-incomplete` or
  `target-unreachable` token and retain an empty findings list
- propagation exposes unresolved-failure status, event previous state and
  qualification, and the ordered unaffected-object list
- every empty public array is represented as `- none`

The JSON schema and field ordering remain `ltfa.assurance/v1`. The repair does
not publish any field classified as core-only by ADR 0003. Existing restricted
selection still fails closed unless `--include-restricted` is explicit.

## Regression evidence

`ltfa.cli.assurance-output-parity` parses JSON and independently reconstructs
the corresponding human representation from every public field. It requires
byte-identical human output for:

- a complete result with structural, SPOF, and propagation details
- a bounded-incomplete result
- an unreachable result with nullable structural and propagation sections

Equivalent human and JSON invocations must also remain byte-stable. Running
this focused test against the `v0.2.0` executable fails with status `1` at the
complete-result parity check; it passes after the repair.

The implementation commit passed:

```text
cmake --preset release
cmake --build build/release -j2
ctest --preset release --output-on-failure
```

Result: 24/24 tests passed, including installed-tree and static/runtime offline
boundary checks.

`perttool document check plans/v0.2.1-cli-consistency.pert
--warnings-as-errors` and `git diff --check` also passed.

No version, tag, release, network feature, command execution, plugin, database,
or secret behavior changed.
