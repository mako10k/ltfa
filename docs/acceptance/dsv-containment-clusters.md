# Semantic DOT Containment Cluster Acceptance

Date: 2026-07-30

Task: `DSV_STRUCTURE`

Status: accepted

## Implemented boundary

The model-output API now distinguishes the byte-stable `raw` DOT view from
semantic presentation views. A semantic view emits `ltfa.diagram.dot/v1` and
records the selected view in graph metadata. The existing default remains
`raw` and continues to emit `ltfa.model.dot/v1`.

Semantic containment rendering:

- builds a deterministic forest from selected, visible `contained_in`
  relations
- folds a relation only when the child has exactly one selected containment
  parent, both objects are in the same scope, the parent is an entity, and the
  parent chain is acyclic
- renders containment parents as nested clusters with visible labels and
  stable identifiers in tooltips
- suppresses only the eligible structural edges represented by cluster
  membership
- retains multi-parent, cyclic, cross-scope, and otherwise ineligible
  containment as explicit dotted structural edges
- uses an internal invisible anchor when a folded container participates in
  another selected edge

Endpoint ownership remains an explicit node-and-edge representation in this
slice. Port folding and removal of synthetic `owns` edges belong to
`DSV_PORTS`.

## Compatibility and safety evidence

The existing raw DOT golden remains byte-identical. Semantic tests cover:

- two-level nested containment
- no duplicate ordinary node for a folded container
- declaration-order independence
- stable label escaping
- default restricted-output omission
- multi-parent, cyclic, and cross-scope fallback
- relation kind metadata without repeated visible edge labels

The renderer remains an offline C++17 text emitter. It does not invoke
Graphviz, access the environment or network, or add a linked or vendored
dependency.

## Validation

```text
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure -R 'ltfa\.model\.output'
ctest --test-dir build --output-on-failure
```

The focused run passed the raw model-output test and both raw and semantic
external Graphviz render tests. The complete Release build passed all 25
tests.

The first complete run found a root-owned generated
`build/install_manifest.txt` left by an earlier administrator installation.
After removing that generated file, the install-smoke test and the complete
suite passed as the normal repository user. No source change was made to mask
the environment residue.
