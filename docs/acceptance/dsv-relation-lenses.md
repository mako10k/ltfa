# Semantic DOT Relation Lens Acceptance

Date: 2026-07-30

Task: `DSV_LENSES`

Status: accepted

## Implemented boundary

The model-output API defines five exact DOT view names:

- `raw`
- `topology`
- `power`
- `dependency`
- `overlay`

`to_string(DotView)` and `parse_dot_view()` round-trip those names. Parsing is
case-sensitive and rejects unknown values, providing the strict value parser
for the later descriptor-backed CLI integration.

The semantic renderer applies the accepted relation inventory:

| View | Normal relation kinds |
| --- | --- |
| `topology` | `network`, `power`, `serial`, `contact`, `analog-current`, `member_of` |
| `power` | `power`, `powered_by` |
| `dependency` | `depends_on` |
| `overlay` | `carried_by`, `uses_underlay`, `terminates_on`, `routes_via` |

Eligible `contained_in` remains a structural cluster primitive in every
semantic view. An ineligible `contained_in` relation remains an explicit
fallback edge. Together with the lossless `raw` view, every built-in
`RelationKind` has an explicit representation.

Semantic edges carry stable relation IDs and kinds in DOT metadata instead of
repeating the kind as every visible edge label. Physical kinds use solid
styles, logical kinds use dashed styles, and containment fallback uses a
dotted style. Each built-in kind has a deterministic color; highlighted
relations retain the existing stable emphasis.

## Compatibility and safety evidence

The raw view still follows the existing branch and golden output. The new
value parser does not alter CLI behavior until `DSV_CLI_DOCS` adds the reviewed
option descriptor and format dependency.

The lens fixture includes all 13 built-in relation kinds. Tests assert the
positive and negative inventory for all four semantic lenses, exact view-name
round trips, unknown-value rejection, and highlighted relation emphasis.

Graphviz remains external. Separate optional tests render topology, power,
dependency, and overlay semantic DOT without adding a linked or vendored
dependency.

## Validation

```text
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure -R 'ltfa\.model\.output'
ctest --test-dir build --output-on-failure
```

The focused model-output run passed the unit test plus raw and all four
semantic Graphviz render tests. The complete Release suite passed all 28
tests.
