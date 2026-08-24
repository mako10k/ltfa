# CLI Language Guide Acceptance Evidence

Date: 2026-08-24

Implementation status: verified

Publication status: not requested or performed

## Implemented Boundary

This change implements CLI Contract 4 under Decision 0015 without changing model semantics,
existing command effects, exit statuses, existing schema identities, or the one-shot offline core
boundary.

Delivered:

- `ltfa guide [topic] [subtopic] --level index|quick|detail --format text|json`
- one immutable catalog for guide validation, rendering, suggestions, and completion
- eight top-level topics covering syntax, statements, identifiers, attributes, includes, samples,
  errors, and workflows
- deterministic links from root and command help to relevant language or workflow pages
- bounded `guide:` and `inspect-guide` recovery actions for DSL, include, and validation errors
- descriptor-backed completion for guide topics, subtopics, levels, and formats
- an installed LTFA DSL v1 reference and validated `minimal` and `resilience` sample trees
- `ltfa.language-guide/v1` structured output

Preserved:

- `help` remains the command contract and `guide` remains the language/workflow contract
- guidance does not load a model, inspect environment state, launch a process, or use the network
- diagnostics do not edit input or automatically retry an operation
- the guide documents only grammar and behavior implemented by the current parser and validator
- model-aware completion, runtime localization, and other shell integrations remain out of scope

## Reference Readback

The design comparison used immutable local source states:

- `llmthink` at `d5b61da00a4af0e86ccd863fa1f46f6caee6d3dd`
- `perttool` at `45a3b3a642ecb141641055ed0a0c858f47a34891`

They are design references only; LTFA has no build-time, runtime, or vendored dependency on them.

## Verification

The following configurations passed from the same implementation state with warnings as errors:

| Configuration | Result |
| --- | ---: |
| GCC Release | 52/52 |
| GCC ASan/UBSan | 41/41 |
| GCC LeakSanitizer | 41/41 |
| Focused guide/navigation/completion/install checks | 5/5 |

The Release suite includes guide text/JSON contracts, suggestions and action ordering, both
normative samples, the installed documentation tree, the bash completion backend and wrapper,
static and runtime offline-boundary checks, the existing model and analysis suite, and external
Graphviz validation. Both sanitizer suites include all new guide, completion, and install tests.

Focused acceptance verifies:

- the public catalog contains exactly the eight documented guide topics
- index, quick, and detail levels select deterministic content
- unknown topics and subtopics provide a bounded correction and valid navigation actions
- root and per-command help expose the relevant guide routes in text and structured output
- DSL failures retain their status and diagnostics while adding language-specific navigation
- completion candidates derive from the command and guide catalogs rather than copied shell data
- source and installed copies of both samples compile successfully
- installed guide output and completion retain their public schemas and candidate sets

`clang-format` and `git diff --check` also passed. No commit, tag, release, remote write, or
dependency publication was performed.

## User-local Installation Readback

The Release tree was installed to `/home/katsumata-m/.local` and read back from that prefix:

- `bin/ltfa` reports `ltfa 0.3.0-alpha.1` and CLI Contract 4
- `help guide --format json` reports the `guide` descriptor and
  `ltfa.language-guide/v1` result schema
- the guide index, `statements relations` detail page, and `samples minimal` JSON page render
  successfully
- both installed sample trees compile successfully through the installed binary
- the installed completion wrapper passes its bash regression and completes `relations`
- malformed DSL exits with status 3 and adds `ltfa guide errors dsl` without changing the
  existing diagnostic
- the installed binary and `build/release/ltfa` have the identical SHA-256 digest
  `b95b737874c48ce235b7f39a22ae088675bcb248804a5cf5acdc72136e56cb8b`

## Claims, Evidence, And Actions

- **C-LANGGUIDE-010 (high confidence):** CLI Contract 4 makes the implemented LTFA language and
  workflows discoverable through synchronized, bounded, offline interfaces.
  - **E-LANGGUIDE-010:** The GCC Release suite passed 52/52, including the complete existing suite,
    offline-boundary checks, and installed-tree verification.
  - **E-LANGGUIDE-011:** GCC ASan/UBSan passed 41/41.
  - **E-LANGGUIDE-012:** GCC LeakSanitizer passed 41/41.
  - **E-LANGGUIDE-013:** Focused acceptance passed 5/5 for guide, navigation, completion, wrapper,
    sample, and install behavior.
  - **E-LANGGUIDE-014:** User-local readback confirmed Contract 4, both installed samples, guide
    completion, status-preserving DSL navigation, and binary identity.
- **A-LANGGUIDE-001 (implemented):** Add CLI Contract 4 and text/JSON guide rendering.
  - References: C-LANGGUIDE-001, C-LANGGUIDE-002, C-LANGGUIDE-010
- **A-LANGGUIDE-002 (implemented):** Connect help, diagnostics, and completion to the guide.
  - References: C-LANGGUIDE-002, C-LANGGUIDE-003, C-LANGGUIDE-010
- **A-LANGGUIDE-003 (implemented):** Add and install the reference and validated samples.
  - References: C-LANGGUIDE-001, C-LANGGUIDE-002, C-LANGGUIDE-010
- **A-LANGGUIDE-004 (implemented):** Verify release, sanitizer, install, and offline acceptance.
  - References: C-LANGGUIDE-001, C-LANGGUIDE-002, C-LANGGUIDE-003, C-LANGGUIDE-010
