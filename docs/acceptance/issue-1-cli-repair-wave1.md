# Issue #1 CLI Repair Wave 1

Date: 2026-07-29

Issue: [#1 Restore assurance CLI consistency, symmetry, and coverage](https://github.com/mako10k/ltfa/issues/1)

Implementation commit: `04577cc6308118b95f0f1a403a618ca244ee8318`

## Accepted tasks

### CLI_SHARED_BUDGET_REPAIR

The `analyze` command now subtracts path-analysis object and relation usage
before invoking SPOF analysis. The configured limits remain the public
cumulative bounds, while SPOF receives only the remaining work allowance.

`ltfa.cli.analyze.budget-acceptance` covers:

- exact complete object and relation boundaries
- one-unit object and relation exhaustion
- path analysis consuming the complete object or relation allowance
- successful deterministic incomplete results without exceeding either limit

### CLI_HELP_AND_ACCEPTANCE_COVERAGE

The descriptor-backed `analyze` usage now names all five budget options.
Structured detailed help verifies the command metadata, exact grammar, option
types, defaults, schemas, and exit statuses.

CLI acceptance coverage now includes every budget option, stable invalid-value
diagnostics, unknown source and target IDs, identical endpoints, objects
outside the selected scope, and an unknown declared failure. The combined
CTest is named `ltfa.cli.acceptance`.

## Evidence

The exact implementation commit passed:

```text
cmake --preset release
cmake --build build/release -j2
ctest --preset release --output-on-failure
```

Result: 23/23 tests passed.

`clang-format --dry-run --Werror app/cli.cpp app/cli_contract.cpp`,
`perttool document check plans/v0.2.1-cli-consistency.pert
--warnings-as-errors`, and `git diff --check` also passed.

The work remains inside the one-shot offline boundary. It adds no probes,
network access, command execution, plugin loading, database access, schema
version, tag, or release publication.
