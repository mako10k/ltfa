# Observation diagnosis onboarding contract review

Date: 2026-08-25

Task: `DESIGN_OBSERVATION_GUIDE`

Status: proposed ADR and pre-implementation tests audited; owner acceptance pending

## Claims

- `C-ONBOARD-001` (high confidence) — ADR 0016 separates LTFA DSL v1 model input from
  `ltfa.observation/v1` JSON Lines and specifies bounded public routes, schemas, Action Navigation,
  completion, compatibility, and offline effects.
- `C-ONBOARD-002` (high confidence) — ADR 0016 makes documentation, help, Guide, manual, and bash
  completion synchronization an explicit implementation and test obligation.
- `C-ONBOARD-003` (high confidence) — The specified test plan covers focused success, failure,
  parity, synchronization, completion, synthetic example, install-tree, offline, and regression
  behavior before implementation begins.
- `C-ONBOARD-004` (high confidence) — Implementation remains blocked on acceptance of the exact
  proposed ADR.

## Evidence

- `E-ONBOARD-001` — ADR 0007 defines the strict observation JSON Lines fields, enums, limits,
  timestamp rules, extension boundary, atomic failure behavior, and prohibition on producers.
- `E-ONBOARD-002` — Current CLI Contract 4 `diagnose` help identifies the observation schema but
  provides only `ltfa guide workflows diagnose` navigation.
- `E-ONBOARD-003` — Current `workflows diagnose` Guide output distinguishes observation JSON Lines
  from LTFA DSL v1 but does not expose an observation schema page or authorable sample.
- `E-ONBOARD-004` — Current structured `LTOBS-001` output has correction and help actions but no
  `inspect-guide` action.
- `E-ONBOARD-005` — Current installation rules include the LTFA DSL v1 manual and the `minimal` and
  `resilience` model samples, but no observation manual or diagnosis sample.
- `E-ONBOARD-006` — ADR 0016 records the exact proposed contract and pre-implementation test plan.

## Reasoning

The decoder contract already exists and is accepted; the gap is safe discovery and installed
onboarding. Extending the current descriptor-backed Guide preserves the established command/domain
separation while allowing help, JSON guidance, diagnostics, and completion to share one bounded
presentation inventory. A separate installed observation manual supplies complete prose, and
parity/synchronization tests keep that prose and repository guidance aligned with the structured
inventory and decoder behavior.

No live data is required to validate the route. A synthetic local model and observation file can
exercise the installed `diagnose` path with explicit time and deterministic evidence-qualified
output.

## Audited contract checklist

| Requirement | ADR 0016 disposition |
| --- | --- |
| Model/observation domain separation | Separate `model-language` and `observation-data` domains |
| Public routes | `guide observations` subtopics and `samples diagnosis` |
| Structured schemas | CLI Contract 5; existing v1 identities plus additive domain/contract data |
| Error navigation | `LTOBS-001..004` add non-executing observation Guide action |
| Bash completion | Descriptor-backed topics, subtopics, and sample; no semantic IDs |
| Documentation synchronization | README and development guidance checked against Guide inventory |
| Manual synchronization | Installed observation reference checked against Guide inventory |
| Installed example | Synthetic model and JSON Lines tree under `examples/diagnosis` |
| Compatibility | Six commands and existing routes, keys, effects, statuses, and schema IDs retained |
| Offline boundary | No producer, probe, command, network, environment, plugin, database, or clock effect |

## Pre-implementation test matrix

| Test group | Required success and failure evidence |
| --- | --- |
| Guide | text/JSON routes, exact domain and contract inventories, unknown-topic navigation |
| Help and errors | diagnose help links; `LTOBS-001..004` ordered text/JSON actions |
| Synchronization | README, development guide, help, Guide, manual, and completion agree |
| Decoder parity | every documented field/kind/outcome accepted or rejected as specified |
| Completion | observation routes and sample; table-free wrapper; semantic IDs disabled |
| Example | synthetic model/diagnosis success and malformed atomic-failure cases |
| Install | manual, sample, executable, Guide, completion, and diagnosis from a temporary prefix |
| Offline | static and runtime checks for help, Guide, completion, and installed diagnosis |
| Regression | GCC/Clang Release, sanitizers, fuzz seeds, and full CTest |

## Actions

- `A-ONBOARD-001` [executed] — Audit ADRs 0007, 0012, 0014, and 0015 and the current CLI,
  documentation, completion, install, and diagnostic surfaces.
- `A-ONBOARD-002` [executed] — Draft ADR 0016 with the exact bounded contract and test plan.
- `A-ONBOARD-003` [pending] — Obtain owner acceptance of ADR 0016.
- `A-ONBOARD-004` [blocked by A-ONBOARD-003] — Implement only the accepted revision.

## Residual risks

- Markdown prose cannot be generated completely from descriptors without harming readability;
  inventory and route parity therefore depend on focused synchronization tests.
- Human root-Guide wording changes to a domain-neutral title and must be covered as an intentional
  CLI Contract 5 change.
- The synthetic example proves installed deterministic behavior, not fitness for any live
  observation producer or operational environment.

## Boundary

This review authorizes no implementation, push, tag, Release, active probe, network access,
producer integration, automatic repair, or retry. The next permitted mutation after this review is
owner acceptance of the exact proposed ADR.
