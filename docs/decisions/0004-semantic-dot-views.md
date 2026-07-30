# ADR 0004: Semantic DOT Views

Date: 2026-07-30

Status: Accepted

## Context

`ltfa.model.dot/v1` is a deterministic, evidence-safe rendering of the selected
normalized graph. Its default scope grouping places every object in one scope
cluster, renders every selected endpoint as a node, adds one synthetic `owns`
edge per endpoint, and renders every selected model relation as an edge.

That representation is useful as a lossless graph-oriented artifact, but it is
not a readable operational diagram for a model with substantial structural
information. The redacted Ebino model demonstrates the distinction:

- 99 entities and 50 endpoints become 149 DOT nodes
- 190 model relations plus 50 endpoint ownership edges become 240 DOT edges
- 70 `contained_in` relations describe spatial hierarchy
- 44 `powered_by` and 35 `depends_on` relations describe separate analytical
  concerns from physical connectivity

The 70 Ebino containment children each have one parent. The 13 objects that act
as containment parents have no non-containment relation in that model, so the
site is a suitable acceptance case for nested clusters. This evidence does not
establish that all future models have single-parent, acyclic containment.

The requirements retain one heterogeneous normalized graph, deterministic DOT,
selected layer or path views, restricted-output safety, and external optional
Graphviz rendering. They do not require every model relation to use the same
visual primitive.

## Decision

### Preserve raw compatibility

The current output remains the `raw` DOT view and retains schema
`ltfa.model.dot/v1`. During this plan it remains the default for
`--format dot`. Existing raw golden output, filtering, focus, highlights,
stable ordering, and restricted-output behavior must remain byte-stable.

Semantic presentation output uses schema `ltfa.diagram.dot/v1` and records its
selected view in graph metadata. A semantic diagram is a deterministic
presentation of model facts, not a lossless replacement for JSON or raw DOT.

The public view names are:

- `raw`
- `topology`
- `power`
- `dependency`
- `overlay`

An unknown view is a CLI usage error. A semantic view option is valid only with
DOT output.

### Represent structure spatially

In semantic views, an eligible `contained_in` relation becomes nested cluster
membership rather than an edge. The child is placed in the cluster represented
by its parent; nested parents produce nested clusters. The cluster label uses
the visible parent label, while stable identifiers remain available as DOT
metadata or tooltips without repeating them in the visible label.

A containment relation is eligible only when:

- both objects are selected and visible under the active restricted-output
  policy
- the child has exactly one selected visible containment parent
- adding the relation does not create a containment cycle

Ambiguous, cyclic, filtered, or otherwise ineligible containment is not
inferred. Its objects remain ordinary nodes and the selected relation falls
back to an explicit structural edge.

A folded container that participates in a selected non-containment relation
uses a deterministic internal anchor inside its cluster. The anchor exists
only in the DOT presentation and never becomes a model object or public
analysis identifier.

### Represent endpoint ownership as ports

An eligible endpoint becomes a stable port on its selected visible owner
instead of an independent node. Relations continue to reference the endpoint
fact but render against the corresponding owner port. Synthetic `owns` edges
are omitted from semantic views.

Port folding is eligible only when the endpoint and owner are both selected and
visible and the owner has one unambiguous rendered representation. Otherwise,
the endpoint remains a node and retains the explicit ownership fallback.

Visible semantic labels prefer the model label. Stable entity and endpoint IDs
remain in deterministic DOT identifiers and tooltips. Evidence references and
restricted attributes remain omitted.

### Separate relation lenses

Structural relations establish spatial organization and are not automatically
mixed with every analytical relation:

| View | Model relation kinds rendered as normal edges |
| --- | --- |
| `raw` | all selected relation kinds, plus current ownership edges |
| `topology` | `network`, `power`, `serial`, `contact`, `analog-current` |
| `power` | `power`, `powered_by` |
| `dependency` | `depends_on` |
| `overlay` | `routes_via`, `terminates_on` |

All semantic views may use eligible `contained_in` relations and endpoint
ownership as structural primitives. An ineligible structural relation remains
an explicit fallback edge so filtering or malformed hierarchy cannot silently
invent placement.

Semantic relation kinds use stable color, line, and arrow styles. Repeated kind
names are not required as visible labels on every edge; the kind remains in
deterministic edge metadata or a legend. Highlighted relations retain a
distinct stable emphasis.

### Determinism and safety

For identical normalized input, output context, restricted policy, selection,
and view, semantic DOT is byte-stable. Cluster, node, port, edge, attribute,
and fallback ordering is derived from stable external identifiers.

Semantic rendering:

- performs no network, process execution, environment lookup, or Graphviz
  library call
- adds no mandatory or vendored dependency
- emits DOT text only
- preserves default restricted omission and explicit restricted inclusion
- does not expose evidence references or raw observation details

### Deferred work

Fixed coordinates, `pos`/`pin`, `neato -n2`, interactive HTML, Cytoscape,
ELK, and other renderer dependencies are excluded. A later task may consume
the semantic view model after nested structure and relation lenses have been
accepted.

## Consequences

- Raw compatibility and machine-readable JSON remain authoritative when every
  relation must be inspected.
- Semantic diagrams become materially smaller by encoding ownership and
  containment without ordinary edges.
- Operators select the concern they need instead of receiving physical,
  power, dependency, and overlay relations in one drawing.
- The renderer needs an explicit containment forest, cluster anchors, port
  identifiers, lens filtering, and fallback tests.
- The descriptor-backed CLI and help must describe the view/format dependency
  exactly before the option is public.

## Acceptance

Acceptance requires:

- raw DOT golden output remains byte-identical
- semantic golden tests cover nested clusters, ports, all lenses, highlights,
  fallbacks, escaping, declaration-order independence, and restricted output
- the optional Graphviz test renders each semantic fixture when `dot` exists
- the redacted Ebino model demonstrates deterministic nested clusters, folded
  endpoint ports, and the expected structural edge reduction
- Release and sanitizer suites pass without network or secret access
