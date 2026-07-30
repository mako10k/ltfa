# Issue #1 CLI Repair Acceptance

Date: 2026-07-30

Status: accepted

Issue: [#1 Restore assurance CLI consistency, symmetry, and coverage](https://github.com/mako10k/ltfa/issues/1)

Validated candidate commit: `470b1c0443098ddd241c4a1368c56d0be3f9a95b`

## Accepted repair

The Issue #1 repair is complete within the existing offline CLI Contract 2 and
`ltfa.assurance/v1` compatibility boundary:

- `04577cc6308118b95f0f1a403a618ca244ee8318` enforces shared cumulative path
  and SPOF work budgets, restores the exact `analyze` help grammar, and fills
  the selection and budget acceptance gaps.
- `323367a0c0dea6ab67b705b761b06d81f0a664bd` defines every public assurance
  field and explicitly dispositions core-only propagation data.
- `2b99c97f674a88a0757499de0fd62e4e4bc1af3c` makes human output a lossless
  deterministic presentation of the JSON inventory and adds the focused
  parity regression.

The focused cumulative-budget tests prove that combined path and SPOF object
and relation usage never exceeds the configured bounds. Complete and
bounded-incomplete results remain successful deterministic findings rather
than process failures.

`ltfa.cli.assurance-output-parity` independently reconstructs human output
from the JSON public fields for complete, bounded-incomplete, and nullable
results. Equivalent text and JSON invocations are byte-stable. Running the
test against the `v0.2.0` executable fails at the complete parity check with
status `1`; the accepted candidate passes.

## Verification matrix

The exact candidate tree was configured, built, and tested with warnings as
errors:

| Configuration | Result |
| --- | ---: |
| Release | 24/24 |
| ASan/UBSan | 21/21 |
| LeakSanitizer | 21/21 |
| ASan/UBSan plus standalone fuzz seeds | 22/22 |

The Release suite includes:

- descriptor-backed root and detailed text/JSON help
- shared-budget boundary and exhaustion regression
- complete, incomplete, unreachable, and restricted assurance CLI behavior
- installed-tree version, help, documentation, model, and assurance execution
- static source and binary offline-boundary checks
- parser, core assurance, and complete-CLI runtime traces with zero network
  syscalls
- external Graphviz validation

The sanitizer configurations rerun the applicable CLI parity, budget,
installed-tree, static offline, and Graphviz gates. The fuzz configuration
also replays the committed parser seed corpus.

Additional checks passed:

```text
clang-format --dry-run --Werror app/assurance_output.cpp app/cli.cpp \
    app/cli_contract.cpp app/main.cpp
perttool document check plans/v0.2.1-cli-consistency.pert \
    --warnings-as-errors
git diff v0.2.0..HEAD --check
```

## Compatibility and safety boundary

- The executable remains `ltfa 0.2.0` pending a separately authorized patch
  release preparation.
- CLI Contract 2 and `ltfa.assurance/v1` remain unchanged.
- Restricted stable IDs still require explicit `--include-restricted`.
- No normalized core-only field was added to assurance output.
- No observation, network, probe, command execution, plugin, database, daemon,
  secret, or external-write behavior was added.

At acceptance, Issue #1 remains open and the repair commits remain local.
Issue closure, pushing, version change, tag creation, and release publication
require separate owner actions.
