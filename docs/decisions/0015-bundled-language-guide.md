# Decision 0015: Bundled LTFA Language Guide

Date: 2026-08-24

Status: accepted

## Context

CLI Contract 3 makes command help, error recovery actions, and bash completion discoverable, but
it describes only how to select a model root and entry file. It does not identify LTFA DSL v1,
show the document header or statement grammar, explain identifier and include constraints, or
navigate from a parser diagnostic to language-specific guidance. Repository test fixtures are not
an installed user interface and are not reachable from command help.

The following current local implementations were reviewed as design references only:

- `llmthink` at `d5b61da00a4af0e86ccd863fa1f46f6caee6d3dd`, whose DSL help uses a
  topic/subtopic hierarchy, index/quick/detail views, related requests, and logical sample IDs
- `perttool` at `45a3b3a642ecb141641055ed0a0c858f47a34891`, whose command `help` and
  domain `guide` are separate descriptor-backed contracts with text and structured output

LTFA does not depend on either project at build time or runtime.

## Decision

### CLI Contract 4

The additive language-discovery boundary is CLI Contract 4. Existing command names, command
effects, exit statuses, output routing, and the `ltfa.command-help/v1` and `ltfa.cli-error/v1`
schema identities remain compatible.

The public language route is:

```text
ltfa guide [topic] [subtopic] [--level index|quick|detail] [--format text|json]
```

Without a topic, `guide` defaults to the index. A topic defaults to quick guidance, and a
topic/subtopic pair defaults to detail guidance. An explicit `--level` always wins. The command is
read-only, uses no document or environment input, and emits either text or
`ltfa.language-guide/v1` JSON.

### Guide catalog

One immutable guide catalog is authoritative for topic validation, text output, JSON output,
related navigation, spelling suggestions, and shell completion. Its top-level topics are:

- `syntax`: document header, lexical form, values, and comments
- `statements`: the nine implemented statement forms and their required fields
- `identifiers`: stable identifier and attribute-key grammar
- `attributes`: statement-specific attributes, values, and extension keys
- `includes`: quoted relative paths, root containment, cycle handling, and bounds
- `samples`: logical sample IDs, embedded excerpts, and repository/installed locations
- `errors`: DSL, include, and model-validation diagnostic families
- `workflows`: validate/render, analyze, and diagnose command sequences

The catalog describes only behavior evidenced by the current lexer, parser, include resolver,
model compiler, and CLI. It does not speculate about a future grammar or expose model-specific IDs.

### Help and error navigation

Root help and the detailed `model`, `analyze`, `cuts`, and `diagnose` help routes link to the most
relevant language or workflow guide. Structured command help adds the same deterministic
navigation.

Handled core language errors retain the existing `next:` and `help:` lines and add a `guide:` line
for the applicable `errors dsl`, `errors include`, or `errors validation` page. Their structured
`actions` retain correction and command-help actions and add a bounded `inspect-guide` command.
The diagnostic never edits input or automatically retries an operation.

### Bundled reference and samples

The source tree and installed documentation contain an LTFA DSL v1 reference plus two normative,
validated sample trees identified as `minimal` and `resilience`. Guide output uses logical sample
IDs and prefix-relative locations rather than assuming a particular installation prefix.

### Completion

The private bash completion query derives `guide` topics, valid subtopics, `--level` values, and
`--format` values from the command and guide catalogs. The bash wrapper remains a thin transport
and carries no copied language table.

## Claims And Evidence

- **C-LANGGUIDE-001:** Separating command help from a domain guide preserves concise command
  discovery while making the DSL learnable.
  - **E-LANGGUIDE-001:** Current `ltfa help model` describes paths and output formats but exposes
    no language name, grammar, statement catalog, minimal document, or language-reference route.
  - **E-LANGGUIDE-002:** The reviewed `perttool` exposes command metadata through `help` and DSL
    usage through a separately versioned `guide` result.
- **C-LANGGUIDE-002:** A hierarchical immutable catalog can serve humans, automation, diagnostics,
  and completion without drift.
  - **E-LANGGUIDE-003:** The reviewed `llmthink` uses topic/subtopic and index/quick/detail views
    with related requests and logical sample IDs.
  - **E-LANGGUIDE-004:** Decision 0002 already establishes immutable descriptors as the source of
    truth for LTFA CLI parsing and discovery.
- **C-LANGGUIDE-003:** Language-specific error navigation can remain bounded and offline.
  - **E-LANGGUIDE-005:** LTFA diagnostics already expose stable `E_DSL_*`, `E_INCLUDE_*`, and model
    validation families without applying a mutation or retry.

## Alternatives

### Add all grammar prose to `ltfa help model`

Rejected because command help would become long, non-hierarchical, and difficult to project into
focused diagnostics or structured consumers.

### Add only `ltfa help language`

Rejected because it conflates the command-contract schema with a domain guide and leaves no clean
topic/subtopic or view-level contract.

### Link only to repository test fixtures

Rejected because fixtures are not installed, are optimized for regression coverage, and are not a
stable user-facing sample identity.

### Load the selected model to provide contextual completion or guidance

Rejected because help and completion must remain cheap, non-semantic, deterministic, and safe for
restricted model IDs.

## Consequences

- The public command catalog gains `guide`, and the CLI contract version becomes 4.
- Guide content is a versioned interface and must change with lexer, parser, or validator grammar.
- The installed package grows by a small Markdown reference and two local sample trees.
- DSL and include errors gain an additional read-only action but preserve existing correction and
  command-help navigation.
- Other shells, runtime localization, and model-aware guidance remain out of scope.

## Implementation Notes

- Add immutable guide descriptors and renderers in `app/language_guide.*` and connect them to the
  command descriptors in `app/cli_contract.*`.
- Dispatch `guide` through normal command parsing in `app/cli.cpp`.
- Extend descriptor-backed completion without adding static candidates to the bash script.
- Install `docs/language/ltfa-dsl-v1.md` and `examples/{minimal,resilience}`.
- Add focused guide, navigation, completion, install-tree, and offline-boundary regressions.

Implementation actions trace as follows:

- **A-LANGGUIDE-001:** Add CLI Contract 4 and `ltfa.language-guide/v1` text/JSON rendering.
  - References: C-LANGGUIDE-001, C-LANGGUIDE-002
- **A-LANGGUIDE-002:** Connect command help, diagnostics, and completion to guide topics.
  - References: C-LANGGUIDE-002, C-LANGGUIDE-003
- **A-LANGGUIDE-003:** Add and install the language reference and validated logical samples.
  - References: C-LANGGUIDE-001, C-LANGGUIDE-002
- **A-LANGGUIDE-004:** Verify release, sanitizer, install, and offline acceptance.
  - References: C-LANGGUIDE-001, C-LANGGUIDE-002, C-LANGGUIDE-003

## Review

The user explicitly approved this implementation direction after requesting comparison with
`llmthink` and `perttool`. The decision does not change model semantics or the offline safety
boundary, so its status is accepted. Implementation and verification evidence is recorded in
[`../acceptance/cli-language-guide.md`](../acceptance/cli-language-guide.md).

## Follow-ups

- Add a guide topic only when the underlying grammar or workflow is implemented and evidenced.
- A future observation-JSON guide requires a separate topic review because it is not LTFA DSL v1.
