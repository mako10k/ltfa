# ADR 0008: Observation Evidence Index

Date: 2026-07-31

Status: Accepted

## Context

The strict JSON Lines decoder preserves observation values but deliberately
does not resolve them against the topology model. Slice 3 next requires an
explicit analysis reference time, registered probe semantics, freshness,
history, and conflicts without consulting a clock or turning a probe result
into an unsupported device-state claim.

The reviewed design says to group observations by probe and vantage, keep every
record at the newest instant, retain older records as history, and distinguish
stale evidence from current state. It also requires conflicting vantage results
to survive arrival order and requires ICMP timeout to mean only missing ICMP
reachability from its declared vantage.

## Decision

`build_evidence_index` is a public, value-oriented core API. It receives a
normalized model, an owned vector of decoded observations, and an explicit
`ObservationTimestamp analysis_at`. It returns either one complete owned index
or one diagnostic and no partial index. `parse_observation_timestamp` exposes
the same strict RFC 3339 profile as observation decoding so callers can resolve
the explicit reference value without reimplementing timestamp parsing. Neither
API reads the system clock or performs I/O.

Every observation must resolve to exactly one registered probe and must match
that probe's target, vantage, kind, and `valid-for` value exactly. Exact validity
matching prevents a producer from widening its registered freshness window.
A custom-heartbeat probe must also declare a non-empty `x-outcome-map` model
attribute. The attribute names the registered producer-side mapping into the
common `pass`, `fail`, `timeout`, `error`, and `unknown` outcomes; LTFA does not
execute the heartbeat or interpret a raw response.

Timestamps are compared as UTC instants, including numeric-offset
normalization, nanoseconds, and the accepted RFC leap-second representation.
An observation after `analysis_at` fails explicitly instead of being silently
used or discarded. Freshness uses the closed interval from `observed_at`
through `observed_at + valid_for_ms`: the exact expiration instant is current,
and the first later instant is stale. Arithmetic compares elapsed seconds and
nanoseconds without adding untrusted validity to a calendar timestamp.

The index sorts observations deterministically by registered probe and vantage,
newest normalized instant first, then stable result and source fields. Each
group exposes contiguous `latest` and `history` ranges. All records at the
newest instant remain in `latest`; no arrival-order overwrite occurs. Every
record, including stale history, retains its own freshness classification.
Downstream diagnosis may treat only fresh records in a group's `latest` range
as current state.

A result signal comprises outcome plus optional metric value/unit and discrete
state. Different signals at the same newest instant within one probe/vantage
group create a `same-probe-and-vantage` conflict. Fresh latest signals for the
same registered target and kind create an `across-vantages` conflict when at
least two vantages and two signals are present. Stale evidence remains visible
but does not create an across-vantage current-state conflict. Conflicts retain
indices for every participating record.

Every ICMP record is typed as `vantage-reachability`; in particular, a timeout
states only that ICMP reachability was not observed from that vantage. The
index has no field that transitions the target, device, link, or power source
to unavailable. Other records are typed as `registered-probe-outcome` and are
also interpreted only by later graph-aware diagnosis.

## Consequences

Evidence selection is reproducible for identical model, observations, and
`analysis_at`, and its working order is bounded by the already bounded decoded
record set. The implementation uses sorting and indexed ranges rather than a
long-running store or database. The CLI still has no observation command, and
this decision does not add diagnosis, output rendering, live probes, monitoring
adapters, network access, process execution, plugins, clock defaults, or secret
retrieval.
