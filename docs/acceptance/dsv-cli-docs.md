# Semantic DOT CLI and Documentation Acceptance

Date: 2026-07-30

Task: `DSV_CLI_DOCS`

Status: accepted

## Public CLI boundary

The descriptor-backed `model` command now exposes:

```text
--dot-view raw|topology|power|dependency|overlay
```

The option defaults to `raw`, is not repeatable, and is valid only with
`--format dot`. The command rejects an unknown value through the shared enum
parser and rejects non-DOT combinations with the stable usage diagnostic:

```text
LTCLI-001 error: option --dot-view requires --format dot
  token: --dot-view
  help: ltfa help model
```

Text help and `ltfa.command-help/v1` JSON are generated from the same
descriptor. Structured help records the default, all five enum values, and
the `format=dot` dependency. Result schemas now inventory:

- `ltfa.model/v1`
- `ltfa.model.dot/v1`
- `ltfa.diagram.dot/v1`
- `ltfa.cli-error/v1`

## Compatibility

Omitting `--dot-view` preserves the existing raw DOT branch. Acceptance tests
compare default DOT with explicit `--dot-view raw` byte-for-byte.

The four semantic values emit `digraph ltfa_diagram` with schema
`ltfa.diagram.dot/v1` and exact `view` metadata. Human and JSON model formats
are unchanged. Supplying `--dot-view` with either format is a usage error,
including structured JSON error output when `--format json` is selected.

README documents compatible raw DOT and semantic topology and power commands.
The development guide includes a direct semantic command and retains Graphviz
as an optional external test/rendering tool.

## Installed-tree evidence

The install-smoke gate now exercises:

- installed version and generated help
- installed `ltfa.model/v1` JSON
- installed semantic topology DOT and its schema/view metadata
- installed assurance JSON

This verifies that the option is present in the installed executable rather
than only in an in-tree test helper.

## Validation

```text
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure \
  -R 'ltfa\.cli\.(help|acceptance)|ltfa\.install\.smoke'
ctest --test-dir build --output-on-failure
```

The focused CLI/help/install tests passed. The complete Release suite passed
all 28 tests.
