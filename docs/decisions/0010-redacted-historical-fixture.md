# ADR 0010: Redacted Historical Structure Fixture

Date: 2026-07-31

Status: Accepted

## Context

The reviewed requirements identify the adjacent operations repository's Ami-A
alarm-topology snapshot as useful historical structure for the offline
prototype. The same review explicitly prohibits copying that operational record
into tests because it contains sensitive and time-specific details. Acceptance
requires a structure-derived fixture that works without the original checkout,
network access, a live site, or secrets.

The source record combines access facts, converter/device-monitor mappings,
historical observations, manual interpretation, open questions, office access
references, and issue/source provenance. Most of those values are neither
necessary nor appropriate for a public deterministic test.

## Decision

The committed fixture under `tests/fixtures/redacted-history/` is a synthetic
derivative, not a sanitized copy. It retains only these reviewed structural
facts:

- five collector mappings terminate on three target-equivalence classes;
- the five mapping fanouts are `11`, `2`, `11`, `2`, and `9` in structural
  order;
- the historical record contains six time-stamped observations and distinguishes
  local and remote observation context; and
- manual interpretation and open questions mean the modeled view must remain
  `partial` with `inferred` evidence quality.

All identifiers, labels, timestamps, outcomes, and relationships in the fixture
are synthetic. The six-record observation scenario is designed specifically to
exercise the reviewed contracts: current local pass versus remote timeout,
same-probe disagreement, retained stale history, and stale remote error. Those
values are test data and are not claims about the historical site.

The transformation deliberately omits every operational value and unrestricted
detail, including:

- site name, site code, address, hostname, and external-exposure notes;
- converter, target, device, database, and legacy identifiers;
- alarm labels, readings, supporting-fact text, summaries, and original times;
- office allowlists, router details, issue and journal identifiers, branch and
  source-file references; and
- evidence references, producer metadata, detail strings, credentials, and
  secret-shaped fields.

The fixture uses synthetic IDs such as `site:fixture`, fixed dates in 2030, and
integer `x-device-count` attributes. It contains no IPv4 address, email address,
operational absolute path, evidence reference, or observation detail.

`ltfa_redacted_history_acceptance_test` owns the durable redaction gate. It
scans the committed fixture for operational and secret-shaped field names and
address/path patterns, requires absent producer and raw-evidence fields, loads
the model from its local fixture root, decodes all six records, builds the
evidence index, and runs diagnosis. A Linux `strace` test separately requires
zero network syscalls for the complete integration path.

As development evidence, the committed payload was also compared locally with
all 106 source-snapshot string values of length four or more. There were zero
source-value substrings in the committed model and observation payload. This
one-time comparison is recorded only as a count; source values are not copied
into this repository or required by its tests.

## Consequences

The fixture preserves useful cardinality, equivalence-class, vantage, history,
conflict, and uncertainty structure while remaining independently runnable and
reviewable. Diagnosis produces a bounded ten-candidate set whose leading remote
path-loss hypothesis is explained by the synthetic observation pattern and
qualified as partial/inferred.

The fixture cannot be used to reconstruct the live topology, targets, device
inventory, credentials, original events, or current state. It does not
authorize a probe, production lookup, CMDB mutation, or claim about the
historical incident. Future fixture refreshes must repeat the structure-only
review and redaction gates; replacing this derivative with an operational
snapshot is prohibited.
