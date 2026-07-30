# Semantic DOT Endpoint Port Acceptance

Date: 2026-07-30

Task: `DSV_PORTS`

Status: accepted

## Implemented boundary

Selected visible endpoints with a selected visible owner now render as stable
ports in semantic DOT:

- ordinary owners use one deterministic HTML table node
- endpoint rows are ordered by stable external endpoint ID
- port names are deterministic within the owner and selected graph
- physical relations attach to the corresponding owner port
- endpoint IDs remain available as port-cell tooltips
- endpoint labels are HTML-escaped; when absent, only the final external-ID
  component is shown while the complete ID remains in the tooltip
- port rendering does not expose evidence attributes
- folded endpoints do not also render as nodes
- folded ownership does not emit synthetic `owns` edges

A containment parent is represented by a cluster rather than an ordinary
entity node. When that container owns endpoints, the ports are placed on its
deterministic internal anchor. Entity relations continue to target the anchor,
while endpoint relations target an explicit port on the same anchor.

Port folding is fail-safe. If an endpoint has an eligible spatial
`contained_in` parent different from its owner, ownership and spatial
placement are ambiguous. That endpoint remains a normal node inside the
selected containment cluster and retains the explicit synthetic `owns` edge.
No alternative parent is inferred.

## Compatibility and safety evidence

The raw DOT branch and golden output are unchanged. Semantic tests cover:

- port folding on two ordinary device owners
- relation attachment to stable ports
- endpoint node and `owns` edge removal
- deterministic declaration-order output
- endpoint tooltip retention
- HTML escaping for visible endpoint labels
- container-anchor ports
- conflicting containment/ownership fallback
- default restricted-output omission

The renderer remains a deterministic offline text emitter. Graphviz is used
only by optional external validation tests and is not linked, invoked, or
loaded by LTFA.

## Validation

```text
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure -R 'ltfa\.model\.output'
ctest --test-dir build --output-on-failure
```

The focused run passed the model-output unit test and all five external
Graphviz checks. The complete Release suite passed all 28 tests.
