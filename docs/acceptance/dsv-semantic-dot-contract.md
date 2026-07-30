# Semantic DOT Contract Acceptance

Date: 2026-07-30

Task: `DSV_CONTRACT`

Status: accepted

## Accepted boundary

ADR 0004 defines a compatibility-preserving semantic DOT boundary:

- `ltfa.model.dot/v1` remains the byte-stable raw view
- semantic output uses `ltfa.diagram.dot/v1`
- public views are `raw`, `topology`, `power`, `dependency`, and `overlay`
- eligible `contained_in` becomes nested cluster membership
- eligible endpoint ownership becomes a device port
- ambiguous structure remains explicit rather than inferred
- relation kinds are separated into physical, power, dependency, and overlay
  lenses
- restricted-output filtering, stable ordering, offline execution, and the
  external optional Graphviz boundary remain unchanged

Fixed coordinates, interactive HTML, external layout libraries, release
metadata, tags, and publication remain outside this plan.

## Evidence

The existing implementation was inspected before the decision:

- default `DotGrouping::scope` creates one flat scope group
- every selected endpoint receives a synthetic `owns` edge
- every selected normalized relation becomes a DOT edge

Before renderer implementation, the accepted lens table was checked against
all 13 built-in `RelationKind` values. The first draft omitted `member_of`,
`carried_by`, and `uses_underlay`. The contract was corrected so `member_of`
belongs to `topology`, while `carried_by` and `uses_underlay` belong to
`overlay`. All built-in relation kinds are now assigned to a semantic lens or
to the structural containment rule.

The redacted Ebino model was compiled with LTFA 0.2.1 and counted without
network or secret access:

```text
entities=99
endpoints=50
relations=190
contained_in=70
powered_by=44
depends_on=35
containment-parent-objects=13
multi-parent-containment-children=0
```

All 13 Ebino containment-parent objects have zero non-containment relations.
This makes them eligible acceptance candidates without turning that
site-specific fact into a general renderer assumption.

## Validation

```text
perttool document check plans/dot-semantic-view.pert --warnings-as-errors
perttool dag analyze plans/dot-semantic-view.pert --schedule both \
  --capacity CODEX=1 --capacity REVIEW=1
perttool dag next plans/dot-semantic-view.pert \
  --capacity CODEX=1 --capacity REVIEW=1
```

The PERT document passes with six tasks and two gates. With `CODEX=1` and
`REVIEW=1`, the expected resource-constrained makespan is 11.167 days.
`DSV_CONTRACT` is the sole recommended starting task.
