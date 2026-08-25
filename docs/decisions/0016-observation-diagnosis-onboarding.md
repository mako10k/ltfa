# Decision 0016: Observation Diagnosis Onboarding

Date: 2026-08-25

Status: Accepted

Owner acceptance: 2026-08-25

## Context

ADR 0007 fixes the strict `ltfa.observation/v1` JSON Lines decoder contract, and ADR 0012 wires
that local input into `ltfa diagnose`. ADRs 0014 and 0015 subsequently add descriptor-backed help,
Action Navigation, bash completion, and an installed LTFA DSL v1 Guide. ADR 0015 deliberately
leaves an observation-JSON Guide for a separate review.

The current onboarding surface is incomplete:

- `ltfa help diagnose` names `ltfa.observation/v1`, but does not enumerate its required fields,
  optional fields, kinds, outcomes, extension boundary, or complete reference.
- `ltfa guide workflows diagnose` says that observations are JSON Lines rather than LTFA DSL v1,
  but has no observation topic or authorable example.
- handled `LTOBS-001` through `LTOBS-004` errors offer correction and command help, but no focused
  Guide action.
- bash completion has no observation topic or diagnosis-sample route.
- the installation contains the LTFA DSL v1 reference and model samples, but no observation manual
  or complete synthetic diagnosis tree.

The owner also requires the repository documentation, command help, bundled Guide, installed
manual, Action Navigation, and bash completion to stay synchronized. Duplicating an observation
schema independently across those surfaces would make drift likely.

## Decision

### CLI Contract 5

Adopt additive CLI Contract 5. Keep the six existing commands, their effects, exit statuses,
output routing, and schema identities. Extend the existing `guide` command instead of adding a
second guide executable or an observation producer.

The Guide root becomes presentation-neutral (`LTFA Guide`) and describes three separate domains:

- `model-language`: LTFA DSL v1 model sources
- `observation-data`: `ltfa.observation/v1` JSON Lines supplied to `diagnose`
- `workflow`: one-shot offline command sequences

Existing Guide topics, subtopics, invocations, JSON keys, and `ltfa.language-guide/v1` remain.
Structured Guide output adds a stable `domain` value and observation pages add an
`observation_contract` object. These additions do not rename or remove existing fields.

### Public observation routes

Add these descriptor-backed routes:

```text
ltfa guide observations
ltfa guide observations format
ltfa guide observations fields
ltfa guide observations semantics
ltfa guide observations errors
ltfa guide samples diagnosis
```

The top-level route defaults to quick guidance; a subtopic defaults to detail, consistently with
ADR 0015. Text and JSON output derive from the same immutable Guide and observation-contract
descriptors. The existing `workflows diagnose` page links to `observations`, `observations errors`,
and `samples diagnosis`.

The observation contract presented by the Guide is exactly:

- schema: `ltfa.observation/v1`
- framing: one UTF-8 JSON object per physical line, with LF or CRLF; no blank interior line and no
  partial decoded batch on failure
- required fields: `schema`, `probe_id`, `target`, `vantage`, `kind`, `outcome`, `observed_at`, and
  `valid_for_ms`
- optional built-in fields: `duration_ms`, `value`, `unit`, `state`, `producer_sequence`,
  `producer_id`, `evidence_ref`, and `detail`
- kinds: `icmp`, `tcp-connect`, `custom-heartbeat`, `metric`, and `discrete-state`
- outcomes: `pass`, `fail`, `timeout`, `error`, and `unknown`
- extensions: lowercase `x-` keys with bounded scalar values only; secret-bearing key names and
  nested arrays or objects are rejected

The Guide and manual must explain that `observed_at` is explicit RFC 3339 input, freshness is
evaluated against explicit `--analysis-at`, a timeout is evidence rather than proof of device
failure, and a next-observation recommendation never executes a probe.

### Help and Action Navigation

`ltfa help diagnose` and its JSON form retain their current contract and add:

- an observation Guide route: `ltfa guide observations`
- the complete installed manual location
- the synthetic diagnosis example route

Handled observation failures `LTOBS-001` through `LTOBS-004` retain their status classes and
correction/help actions. They add an ordered, non-executing `inspect-guide` action targeting
`ltfa guide observations errors`. Text output adds the corresponding `guide:` line. Navigation
does not edit a file, retry diagnosis, infer a schema repair, obtain observations, or run a probe.

### Descriptor-backed completion

The existing private `__completion --bash` transport derives `observations`, its valid subtopics,
and `samples diagnosis` from the Guide catalog. The bash wrapper remains a thin transport and
copies no observation fields, enums, or routes. Observation filename completion remains ordinary
local file completion; model IDs, probe IDs, target IDs, and semantic JSON content are not
completed.

### Documentation, Guide, and manual synchronization

One immutable observation-presentation descriptor is authoritative for:

- Guide topic validation and text/JSON rendering
- the structured field, kind, outcome, schema, and navigation inventories
- command-help navigation
- Guide and sample completion candidates

The decoder remains the semantic authority fixed by ADR 0007. Focused contract tests enumerate
the presentation descriptor through decoder success and failure cases so a presentation change
cannot silently diverge from decoder behavior.

Repository documentation consists of `README.md` for users and `docs/development.md` for build and
test operators. The complete observation manual is
`docs/observations/ltfa-observation-v1.md`, installed as
`share/doc/ltfa/docs/observations/ltfa-observation-v1.md`. A synchronization test compares the
Guide JSON inventory with the manual and checks that README, development guidance, text/JSON help,
Guide navigation, and the installed location all name the same schema, public routes, and sample.
Documentation prose may add explanations, but it may not introduce a field, enum, effect, or
recovery action absent from the accepted descriptor and decoder contract.

### Installed synthetic diagnosis example

Add `examples/diagnosis/main.ltfa` and `examples/diagnosis/observations.jsonl`. The tree uses only
synthetic stable IDs, explicit timestamps, and bounded local data. It is installed below
`share/doc/ltfa/examples/diagnosis` and is reachable as `ltfa guide samples diagnosis`.

The example must pass `model` and `diagnose` from both the build tree and a temporary installed
prefix. Its expected result is evidence-qualified and deterministic; it must not claim a confirmed
live root cause.

### Offline and scope boundaries

All new routes are one-shot and read-only. Guide, help, completion, manual inspection, and the
synthetic example add no network probes, observation producers, command execution, plugins,
databases, environment discovery, implicit clocks, secret retrieval, SDK installation, or live
target access. LTFA reads only explicitly selected local files. Other shells, schema authoring
tools, automatic repairs, active probes, streaming input, and a new observation schema are out of
scope.

## Claims And Evidence

- **C-OBSONB-001 (high confidence):** A separate observation-data Guide domain can make diagnosis
  authorable without presenting JSON Lines as LTFA DSL v1.
  - **E-OBSONB-001:** ADR 0007 already fixes the complete observation schema and offline decoder
    boundary.
  - **E-OBSONB-002:** Current `workflows diagnose` output explicitly says the observation file is
    not LTFA DSL v1, but exposes no observation topic.
- **C-OBSONB-002 (high confidence):** One presentation descriptor plus decoder-parity and document
  synchronization tests can keep help, Guide, completion, documentation, and manual inventories
  aligned.
  - **E-OBSONB-003:** ADRs 0014 and 0015 already use immutable descriptors for command discovery,
    Guide rendering, Action Navigation, and completion.
  - **E-OBSONB-004:** Current completion candidates are projected from command and Guide catalogs
    rather than copied into the bash wrapper.
- **C-OBSONB-003 (high confidence):** Observation error navigation can be more specific without
  granting retry or probe authority.
  - **E-OBSONB-005:** Current `LTOBS-001` output contains only `correct-and-retry` and
    `inspect-help`; an additive read-only Guide action requires no new input effect.
- **C-OBSONB-004 (high confidence):** A synthetic installed example can validate the complete
  onboarding route without a live system.
  - **E-OBSONB-006:** Existing install smoke already verifies the executable, documentation,
    Guide, completion, and installed model samples from a temporary prefix.
  - **E-OBSONB-007:** ADRs 0007 and 0012 require explicit local observations and explicit analysis
    time and prohibit producer execution.

## Alternatives

### Put the JSON schema into `ltfa help diagnose` only

Rejected because command help would become a second manual and would not provide focused error,
completion, installed-reference, or sample navigation.

### Describe observations only in Markdown

Rejected because help, Guide, Action Navigation, and completion could drift from the manual with
no structured inventory to test.

### Generate all user prose from C++ descriptors

Rejected because readable manuals need examples and explanatory semantics. The accepted boundary
generates structured inventories and enforces parity while allowing reviewed prose around them.

### Add an observation producer or automatic repair command

Rejected because onboarding does not authorize system access, probe execution, changed input, or
an automatic retry.

### Reuse the LTFA DSL `syntax` and `attributes` topics

Rejected because JSON field grammar, JSON Lines framing, timestamp semantics, and extension limits
are a distinct data contract.

## Consequences

- CLI Contract 5 adds Guide content and navigation while preserving existing commands and schema
  identities.
- The observation contract becomes directly discoverable in text, JSON, bash completion, source
  documentation, and an installed manual.
- Repository and installed packages gain one small manual and one synthetic diagnosis tree.
- Decoder or presentation changes must update parity and synchronization tests in the same change.
- Human root-Guide wording changes from DSL-specific to domain-neutral; existing DSL routes and
  content remain available.
- No new operational authority or runtime dependency is introduced.

## Pre-implementation Test Plan

- **Success:** exact text and `ltfa.language-guide/v1` JSON for every observation route; required
  and optional fields, kinds, outcomes, domain, related routes, and manual path are deterministic.
- **Failure:** unknown observation topics/subtopics suggest only valid descriptor entries;
  `LTOBS-001` through `LTOBS-004` expose correction, observation Guide, and command-help actions in
  that order without executing any action.
- **Parity and synchronization:** descriptor inventories match decoder acceptance, README,
  development guidance, diagnose help, Guide JSON, manual, and bash candidates.
- **Completion:** topic, subtopic, sample, and existing enum/path modes pass through the private
  descriptor-backed endpoint; semantic IDs remain disabled.
- **Example:** the synthetic diagnosis tree passes model validation and deterministic diagnosis;
  malformed observation variants fail without a partial result.
- **Install:** a temporary prefix contains the manual, sample, completion script, and executable;
  installed help, Guide, completion, model, and diagnosis routes pass.
- **Offline:** static symbol checks and runtime tracing cover help, Guide, completion, and installed
  diagnosis; no network or process-launch effect is observed.
- **Regression:** complete GCC and Clang Release suites, sanitizer suites, and fuzz seeds pass
  before candidate acceptance.

## Implementation Notes

- Extend `app/language_guide.*` with the observation-presentation descriptors and domain metadata.
- Extend `app/cli_contract.*` with CLI Contract 5 navigation and diagnostic routing.
- Keep `completions/ltfa.bash` table-free.
- Add the installed manual and synthetic example through target-scoped CMake install rules.
- Add focused Guide, error-action, synchronization, completion, install, decoder-parity, and
  offline regressions before updating acceptance evidence.

Proposed implementation actions trace as follows:

- **A-OBSONB-001 (proposed):** Add observation Guide descriptors and structured contract output.
  - References: C-OBSONB-001, C-OBSONB-002
- **A-OBSONB-002 (proposed):** Synchronize diagnose help, Action Navigation, completion,
  documentation, and the installed manual.
  - References: C-OBSONB-002, C-OBSONB-003
- **A-OBSONB-003 (proposed):** Add and install the synthetic diagnosis example.
  - References: C-OBSONB-001, C-OBSONB-004
- **A-OBSONB-004 (proposed):** Run the pre-implementation test matrix and record bounded evidence.
  - References: C-OBSONB-002, C-OBSONB-003, C-OBSONB-004

## Review

The owner accepted the exact proposed contract and test plan at Git revision
`a154d23215d656d15e1780802446992959880dbf` on 2026-08-25. PERT receipt
`RCPT_GUIDE_OWNER_ACCEPTED_20260825` records that acceptance. This post-acceptance status update
does not alter the accepted boundary. Implementation remains limited to the actions and tests in
this record.

## Follow-ups

- Zsh, Fish, and PowerShell completion remain separate decisions.
- Schema migration, streaming input, producer adapters, active probes, or automatic repair require
  separate ADRs and authority.
- Installing the C++ core as an SDK remains outside this decision.
