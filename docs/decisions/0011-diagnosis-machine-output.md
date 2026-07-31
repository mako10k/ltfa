# ADR 0011: Diagnosis Machine Output

Date: 2026-07-31

Status: Accepted

## Context

Slice 3 already has a strict observation decoder, explicit-time evidence
index, bounded deterministic diagnosis, and a synthetic structure-derived
historical fixture. Those tasks intentionally stopped before serialization.
The remaining acceptance condition requires identical normalized model,
observations, reference time, and budgets to produce byte-stable JSON and DOT
without exposing unrestricted observation detail or changing the CLI contract.

The output must preserve the difference between evidence and inference. A
timeout remains an observation from one vantage, a finding remains a ranked
hypothesis, and bounded exhaustion remains a successful but incomplete
analysis result.

## Decision

The public core adds `render_diagnosis_json` and `render_diagnosis_dot` in
`ltfa/diagnosis_output.hpp`. Both receive only an already normalized model,
validated evidence index, diagnosis result, and caller-supplied input
identities. They read no file, clock, environment value, network endpoint,
process, plugin, database, or credential source.

JSON uses schema `ltfa.diagnosis/v1`. Its stable top-level inventory is:

- operation, explicit `analysis_at`, sorted unique model and observation input
  identities, and complete/incomplete/invalid status;
- normalized evidence summaries containing only registered identity, kind,
  outcome, time, validity, freshness, meaning, and conflict state;
- the visible causal cone and ordered findings;
- per-finding hypothesis, rank/tie, score counters, qualification, affected
  objects, assessed evidence, conflicts, and remaining unknowns;
- stable incomplete reasons, warnings, resource-usage counters, and omission
  counts.

DOT uses schema `ltfa.diagnosis.dot/v1` and renders one selected ordered finding
at a time. It shows the hypothesis target, affected and unknown objects,
assessed observation summaries, and conflict relationships. The graph is an
explanation view, not a lossless observation archive or a confirmed-fault
diagram. Graphviz remains external; the core emits DOT text only.

Both renderers sort and deduplicate caller-supplied input identities and sort
set-like object and evidence collections. They preserve the diagnosis engine's
rank order. Reversing semantically identical observation arrival order must
therefore leave output bytes unchanged.

The default restricted-output policy omits restricted objects, observations
whose registered target or vantage is restricted, and findings targeting a
restricted object. An explicit policy may reveal those registered model
identities. Neither policy ever emits optional observation values, unit/state,
duration, producer metadata, evidence references, detail strings, or extension
fields. Stable omission counts and one generic warning expose that content was
withheld without copying it into output.

## Consequences

Slice 3 now has deterministic machine serialization suitable for offline
adapters and acceptance evidence. Conflicting site/remote observations remain
visible as separate evidence; the leading result is still labeled as a
hypothesis with partial/inferred qualification and no numeric probability.

This decision adds no diagnosis CLI command, human renderer, live producer,
automatic probe, persistent observation store, Graphviz linkage, or release
mutation. A future CLI surface must separately define operands, help, error
routing, standard-input behavior, output selection, and compatibility policy.
