# Ebino Semantic DOT Acceptance

Date: 2026-07-30

Task: `DSV_EBINO_ACCEPTANCE`

Status: accepted

## Acceptance input

The read-only smoke used the redacted Ebino model under
`/home/katsumata-m/ops/src/ltfa/ebino`. No live system, network endpoint,
credential, raw device identifier, or observation source was accessed.

Input SHA-256:

```text
5470f98c6325c389bbfbf284ca578ee5b0e4c6ba4a974010ecca2b408d5ac228  main.ltfa
445ffd26f04cca4c8bc4762274b8cee399ad648348f45cf0713866960b74ff4a  completed-diagram.ltfa
dd60014ae759934f2671c241c4625b29118f0e3c585a9895219ef6d2891589a3  cmdb-vpn.ltfa
```

The final Release install-smoke executable reported `ltfa 0.2.1` and had
SHA-256:

```text
9848cfdaa2c31419be57808b784ad60caa6cd74394ffeedcc83650df07eae6b1
```

This version string identifies the current released baseline. Semantic DOT is
an unreleased source-tree addition in this plan; release metadata, version
change, tag, and publication remain outside this acceptance.

## Determinism and Graphviz

The installed Release executable emitted each semantic view twice. Each pair
was byte-identical:

```text
topology    c950d710e62adb500fff5618544545fd876b8cdabdbb6614367df6bb687afc6b
power       3dfb5fd2c116158a061b4484c1f2ffd941824969a1dc3eccd8d7b4d213d6831f
dependency  29a64e865da7dcb5d52d05e199bde5ee39427680068f9cc5bbc14986128471bc
overlay     76b350531954cab59b163c9acd99406457c7dc3e98afbbb649a4f84df56cb3f1
```

The compatible raw DOT remained:

```text
ced8fa1f6ff2b64a9fcef8fab7e30533e89116653fb98f8fea6b5a88874abbd0
```

Graphviz 2.43.0 accepted raw and all four semantic outputs. Generated DOT,
SVG, and PNG files were kept only in a temporary directory and were not added
to either repository.

## Structural reduction

The source model contains 99 entities, 50 endpoints, and 190 model relations.
The raw presentation contains 149 nodes and 240 edges: 190 model relations
plus 50 synthetic ownership edges.

| View | Container clusters | Endpoint ports | Rendered edges | Edge reduction from raw |
| --- | ---: | ---: | ---: | ---: |
| `topology` | 13 | 50 | 31 | 87.1% |
| `power` | 13 | 50 | 44 | 81.7% |
| `dependency` | 13 | 50 | 35 | 85.4% |
| `overlay` | 13 | 50 | 10 | 95.8% |

All 70 selected `contained_in` relations were represented by cluster
membership with zero containment fallback edges. All 50 endpoint ownership
facts were represented by ports with zero synthetic `owns` edges.

## Visual review

The initial semantic topology layout using Graphviz's default top-to-bottom
ranking was 17,205 by 843 pixels and remained impractically wide. The accepted
semantic output records `rankdir=LR`, causing unconnected siblings to stack
while the facility chain remains traceable. Endpoints without a model label
show only their final ID component, while the complete stable ID remains in
the tooltip.

On this host the final topology PNG was 1,425 by 5,223 pixels. It presents the
extra-high-voltage station and substations 1 through 5 as nested facility and
panel clusters in a vertical sequence. This dimension is renderer- and
font-dependent; the accepted contract is the deterministic DOT structure and
orientation, not a fixed pixel coordinate.

Fixed positions, `pos`/`pin`, `neato -n2`, interactive HTML, and a new layout
dependency remain deferred.

## Validation matrix

```text
Release                         28/28 passed
ASan + UBSan                    25/25 passed
LSan                            25/25 passed
ASan + UBSan fuzz-seed replay   26/26 passed
```

The Release suite includes installed-tree smoke, static offline-boundary
checks, three Linux runtime offline checks, raw Graphviz rendering, and all
four semantic Graphviz views. Focused tests also cover raw byte compatibility,
declaration-order independence, restricted-output omission, escaping,
highlighting, multi-parent/cyclic/cross-scope containment fallback, endpoint
ownership fallback, exact CLI help, invalid values, and the DOT-only option
dependency.
