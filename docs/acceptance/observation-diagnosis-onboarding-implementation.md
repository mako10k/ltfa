# Observation Diagnosis Onboarding Implementation

Date: 2026-08-25

Issue: [#2 Add reproducible CI and complete operator onboarding](https://github.com/mako10k/ltfa/issues/2)

Task: `IMPLEMENT_DIAGNOSIS_ONBOARDING`

Status: implementation and local acceptance checks passed; candidate validation remains separate

## Claims

- `C-OBSIMPL-001` (high confidence) — CLI Contract 5 exposes observation JSON Lines as a separate
  `observation-data` Guide domain while retaining all existing command and schema identities.
- `C-OBSIMPL-002` (high confidence) — One immutable observation descriptor supplies the structured
  schema inventory and diagnose-help navigation, while the Guide catalog supplies the matching
  public routes and bash candidates.
- `C-OBSIMPL-003` (high confidence) — `LTOBS-001` through `LTOBS-004` now provide ordered local
  correction, focused observation Guide, and diagnose-help navigation without executing an action.
- `C-OBSIMPL-004` (high confidence) — The source and temporary installed trees both provide a
  deterministic synthetic model-and-observation example that completes offline diagnosis.
- `C-OBSIMPL-005` (high confidence) — Focused and complete GCC Release checks passed without adding
  network, process-launch, producer, environment, plugin, database, secret, or implicit-clock
  effects.

## Evidence

- `E-OBSIMPL-001` — `app/language_guide.*` defines the immutable observation presentation
  descriptor, observation Guide pages, domain metadata, structured inventories, manual route, and
  sample route. `app/cli_contract.cpp` reads that descriptor for diagnose help and observation
  diagnostic navigation.
- `E-OBSIMPL-002` — `tests/cli_observation_onboarding_test.cmake` checks every observation Guide
  route in text and JSON, exact required and optional fields, kinds, outcomes, typo recovery,
  diagnose-help navigation, descriptor-backed completion, `LTOBS-001..004`, restricted output,
  synchronized documents, and the synthetic input variants.
- `E-OBSIMPL-003` — `tests/observation_decoder_test.cpp` retains success and failure coverage for
  all documented kinds and outcomes, complete optional records, required/schema failures, field
  constraints, framing, atomic rejection, UTF-8, timestamps, extensions, and limits.
- `E-OBSIMPL-004` — `ltfa.install.smoke` verifies the installed executable, observation manual,
  diagnosis sample, Guide JSON, completion discovery, model validation, and
  `ltfa.diagnosis-cli/v1` result from a temporary prefix.
- `E-OBSIMPL-005` — `ltfa.boundary.observation-onboarding-offline-runtime` traced help, Guide,
  completion, and diagnosis with `strace -e trace=network`; each trace contained zero network
  syscalls. The repository static offline boundary also passed.
- `E-OBSIMPL-006` — GCC Release configured and built with warnings as errors, and the complete CTest
  suite passed 55/55. That suite included the focused onboarding, install-smoke, static and runtime
  offline, existing diagnosis, bounded-incomplete, restricted-output, Graphviz, and decoder tests.
- `E-OBSIMPL-007` — `git diff --check` passed after C++ formatting with the repository
  `.clang-format`.

## Reasoning

The decoder remains the semantic authority established by ADR 0007. The new descriptor projects
the accepted inventory and navigation into Guide/help output; the existing Guide catalog projects
completion topics and subtopics. Focused tests compare those structured inventories with the
manual and repository guidance, then exercise decoder and CLI failures, preventing a changed
presentation from silently becoming an unsupported authoring contract.

The installed example uses only synthetic stable IDs, a registered probe, one bounded local JSON
Lines record, and an explicit reference time. Its successful source-tree and install-tree results
therefore demonstrate onboarding and packaging, not a live diagnosis claim.

## Actions

- `A-OBSIMPL-001` [executed] — Implement the observation descriptor, Guide domain and routes, CLI
  Contract 5 navigation, Action Navigation, and descriptor-backed completion discovery.
- `A-OBSIMPL-002` [executed] — Add and install the complete observation manual and synthetic
  diagnosis example.
- `A-OBSIMPL-003` [executed] — Synchronize README, development guidance, help, Guide, manual,
  completion, install behavior, and focused success/failure coverage.
- `A-OBSIMPL-004` [executed] — Run the complete GCC Release CTest suite, focused install and offline
  checks, formatting, and diff validation.
- `A-OBSIMPL-005` [pending separate task] — Run the complete candidate matrix and record hosted CI
  and qualified aarch64 results under `VERIFY_V030A2_CANDIDATE`.

## Residual Risks And Boundary

- Markdown prose is not generated from C++; synchronization tests enforce exact public
  inventories and routes, while semantic prose still requires review.
- The local implementation gate does not substitute for the planned Clang Release, sanitizer,
  fuzz-seed, hosted-CI, or aarch64 candidate matrix.
- The remote branch still identifies the prior CI baseline. This record does not authorize a push,
  tag, GitHub Release, stable promotion, or other remote mutation.
- The implementation adds no observation producer, active probe, command execution, automatic
  repair, implicit retry, streaming input, or new observation schema.
