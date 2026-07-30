# ADR 0005: Links, Aggregates, and Protection Domains

Date: 2026-07-30

Status: Accepted

## Context

The normalized model currently represents a physical connection only as a
relation between two objects. A relation can carry evidence metadata, but it
cannot itself be the target of a failure, a failure-domain member, an
availability provider, or a member of another topology object.

That boundary is insufficient for several common communication designs:

- two or more physical links form a bond or link aggregation group
- a link aggregation remains usable while a declared minimum number of members
  remains usable
- physical loops remain present while STP, RSTP, MSTP, or a ring protocol
  selects a normal forwarding topology
- a blocked or standby segment is healthy physical infrastructure, not a failed
  link
- the same physical member may participate in a spatial hierarchy, an
  aggregate, and a protection domain

Encoding these facts as arbitrary extension attributes would make them visible
but inert. Encoding every fact as another ordinary edge also produces a diagram
that obscures the physical topology.

## Decision

LTFA DSL v1 gains three additive entity kinds:

- `link`: one point-to-point physical communication link
- `aggregate`: one logical bond or link aggregation
- `control-domain`: one STP or ring protocol domain

### Physical links

A `link` is a communication-plane, physical-layer entity. It is connected to
exactly two distinct network peers with ordinary `network` connections:

```text
entity link:sw1-sw2:a kind=link plane=communication layer=physical scope=site:a
connect sw1:port1 -- link:sw1-sw2:a kind=network
connect link:sw1-sw2:a -- sw2:port1 kind=network
```

The link entity can be the target of `failure ... mode=link-down`, can carry one
or more `failure-domain=` attributes, and is traversable by the existing
relation graph. Existing direct endpoint-to-endpoint connections remain valid.

Shared media continue to use `bus` or `network-space`; `link` is deliberately
limited to a point-to-point object.

### Aggregates

An `aggregate` is a communication-plane, L2 entity. It declares its mechanism
with `protocol=lacp|static|bonding` and its forwarding policy with
`mode=active-active|active-backup|balance-xor|broadcast`.

Physical link entities become structural members with `member_of`. Availability
policy is declared independently with a requirement group:

```text
entity lag:sw1-sw2 kind=aggregate plane=communication layer=l2 scope=site:a protocol=lacp mode=active-active
relate link:sw1-sw2:a -> lag:sw1-sw2 kind=member_of
relate link:sw1-sw2:b -> lag:sw1-sw2 kind=member_of
require lag:sw1-sw2 by link:sw1-sw2:a group=members mode=at-least minimum=1
require lag:sw1-sw2 by link:sw1-sw2:b group=members mode=at-least minimum=1
```

Structural membership does not silently imply availability. Keeping the
requirement explicit prevents LTFA from inventing a `min-links` policy.
The CLI assurance view likewise excludes `member_of` and `contained_in` from
path and SPOF traversal; those relations describe grouping and placement, not
forwarding connectivity.

`mode=at-least minimum=N` evaluates a provider set as follows:

- `available` when at least `N` providers are available and no provider has a
  known degraded or unavailable state
- `degraded` when the threshold remains met but a provider is known degraded or
  unavailable, including when degraded providers are needed to reach `N`
- `unavailable` when even available, degraded, and unknown providers together
  cannot reach `N`
- `unknown` otherwise

`minimum` is required for `at-least`, forbidden for `all` and `any`, and must be
between one and the normalized provider count.

### Protection and loop-control domains

A `control-domain` is a communication-plane entity at the physical or L2 layer.
Its `protocol` is one of `stp`, `rstp`, `mstp`, `erps`, `mrp`, `turbo-ring`, or
`other`.

Participants use `member_of`. Domain-specific design roles and expected normal
forwarding states belong to that membership relation, not to the physical
connection globally:

```text
entity ring:plant kind=control-domain plane=communication layer=l2 scope=site:a protocol=turbo-ring
relate switch:1 -> ring:plant kind=member_of role=manager
relate link:5-1 -> ring:plant kind=member_of role=backup normal-state=standby
```

Supported roles are `root`, `designated`, `alternate`, `backup`, `manager`,
`member`, `active`, `standby`, and `unknown`. Supported normal states are
`forwarding`, `learning`, `discarding`, `blocking`, `standby`, and `unknown`.
These fields describe the registered design or baseline. They are not live
observations. A future observation contract may report time-qualified current
state without changing the physical model.

### Diagram lenses

The existing `topology` view remains the physical, loss-preserving semantic
lens. Two additive views use membership as presentation structure:

- `logical` folds single-parent `member_of` relations whose target is an
  `aggregate`
- `protection` folds single-parent `member_of` relations whose target is a
  `control-domain`

Each view uses a separate cluster hierarchy because Graphviz clusters cannot
represent overlapping aggregate, spatial, and protection memberships. Multiple
or cyclic memberships fail open as explicit edges instead of choosing a parent.
The `protection` view styles network segments associated with
`discarding`/`blocking`/`standby` members as non-forwarding context; it does not
classify them as unavailable.

Fixed coordinates and live-state rendering remain outside this decision.

## Compatibility

All changes are additive within DSL v1:

- existing entity kinds, direct connections, and `all`/`any` requirements retain
  their behavior
- the raw DOT view and its schema remain unchanged
- semantic output remains `ltfa.diagram.dot/v1`
- model and analysis remain bounded, deterministic, one-shot, and offline

Adding fields to normalized output does not authorize inference. Missing
aggregate policy or control-state evidence remains explicit rather than being
guessed.

## Consequences

LTFA can target a physical link with failures and failure domains, express
K-of-N aggregate availability, and render logical aggregation separately from
loop-control domains. Physical topology, normal forwarding design, availability
dependency, and future observed state remain distinct.

Models that adopt first-class point-to-point links use two network relations per
link. A future DSL sugar may shorten that spelling without changing normalized
semantics.
