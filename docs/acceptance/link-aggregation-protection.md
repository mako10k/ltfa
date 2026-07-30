# Link Aggregation and Protection-Domain Acceptance

Date: 2026-07-30

Decision: `docs/decisions/0005-link-aggregation-and-protection-domains.md`

Status: accepted

## Accepted boundary

This slice adds:

- typed `link`, `aggregate`, and `control-domain` entities
- fail-closed point-to-point link validation
- link failure and failure-domain targeting through the existing entity model
- `mode=at-least minimum=N` requirement compilation and availability evaluation
- aggregate protocol and mode validation
- STP and ring protocol validation
- domain-specific `role` and `normal-state` membership metadata
- `logical` and `protection` semantic DOT views
- CLI assurance filtering that excludes grouping and placement relations from
  forwarding path and SPOF traversal

`normal-state` is registered design state. No live observation producer, network
probe, command execution, or automatic protocol inference was added.

## Focused evidence

The synthetic model at `tests/fixtures/resilience/main.ltfa` compiles to:

```text
entities=6
endpoints=4
relations=10
requirement-groups=1
failure-domains=2
failure-modes=2
```

Its aggregate requirement is normalized as:

```text
mode=at-least
minimum-providers=1
providers=[link:1,link:2]
```

The compiler accepts each point-to-point link only with two distinct
bidirectional network peers. Focused diagnostics cover a one-ended link,
incorrect link layer, incomplete aggregate declaration, missing threshold,
threshold above provider count, and `minimum` on a non-threshold mode.

Availability tests cover:

- threshold met with every member healthy: `available`
- threshold met after known member loss: `degraded`
- threshold met only with a degraded member: `degraded`
- threshold dependent on unknown evidence: `unknown`
- threshold impossible: `unavailable`

The CLI failure smoke for `failure:link-1` reports `link:1` immediately
unavailable and `lag:uplink` immediately degraded. Structural output contains
network connectivity but no `member_of` bridge and no control-domain SPOF.
Excluded grouping and placement relations do not claim that a forwarding
alternative was omitted.

## Rendering evidence

Graphviz accepts both new deterministic semantic lenses:

```text
ltfa.model.output.semantic-logical-graphviz
ltfa.model.output.semantic-protection-graphviz
```

The logical view folds the two member links under an aggregate cluster labeled
with `lacp / active-active / min 1/2`. The protection view folds registered
members under an `rstp` control-domain cluster. The alternate/discarding member
and its adjacent network segments remain present with dashed gray styling; they
are not rendered as failed.

Generated DOT, SVG, and PNG review artifacts remain under the ignored `build/`
tree and are not release inputs.

The final reviewed DOT hashes were:

```text
7118a09f558f2474a22f5808200f6ecd613487ee355222328cff2efc2d24c9f7  resilience-logical.dot
31107b20a17181a8c946aba364c436ea2c8bbbba6934b4b87c0d6eda036b2315  resilience-protection.dot
```

## Validation

```text
cmake --preset release
cmake --build --preset release
ctest --preset release
git diff --check
```

The complete suite passes 30 of 30 tests, including CLI, install, offline
boundary, deterministic output, and all available Graphviz checks.
