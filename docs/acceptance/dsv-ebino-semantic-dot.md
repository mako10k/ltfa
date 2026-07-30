# Ebino Semantic DOT Acceptance

Date: 2026-07-30

Task: `DSV_EBINO_ACCEPTANCE`

Status: accepted

## Acceptance input

The read-only smoke used the redacted Ebino model under
`/home/katsumata-m/ops/src/ltfa/ebino`. No live system, network endpoint,
credential, raw device identifier, or observation source was accessed.

The acceptance was repeated after correcting the documented optical topology
from the completed-deliverables optical trunk drawing dated 2023-12-25. The
model now records each of the five optical segments as a first-class `link` and
groups the five links and five RingSWs into a `control-domain`. The protocol is
`other`; no manager/root, blocked segment, internal forwarding, or live state is
inferred. This is documented topology, not evidence that current field wiring,
Turbo Ring configuration, or failover operation is healthy.

Input SHA-256:

```text
83f57c7ea529b8d1b7997a199a117ae51e0d552a6282e10c08b4c72d09ecd7a3  main.ltfa
0ad12f19f68b38adb661f8d5eccd7bc1dd5f0f4237e038902d3eaad68b659757  completed-diagram.ltfa
dd60014ae759934f2671c241c4625b29118f0e3c585a9895219ef6d2891589a3  cmdb-vpn.ltfa
```

The source-tree Release executable reported `ltfa 0.2.1` and had SHA-256:

```text
baafe1df87fd5b01eb3a857621d36225840c204bc34646f3813026fadebf8b47
```

The version string still identifies the released baseline. Semantic DOT and
ADR 0005 resilience entities are unreleased source-tree additions; release
metadata, version change, install, tag, and publication remain outside this
acceptance.

## Determinism and Graphviz

The installed Release executable emitted each semantic view twice. Each pair
was byte-identical:

```text
topology    def3bf64511aef469595a6fad27902a67d10d3fad6d33ad89d62b7d0d5c5d593
logical     8760d9bd88f2c84300d90072a989e0936b30c84f2630abbca81425043237451a
protection  44661eff3b3163d5de6d7086f53b510bfe635913ce8c46584e6f1f4628b8536c
power       d432bf2b7d0532b8d3ec5fd4efa459853b227c96e47724f0bdf9122f0ea8061e
dependency  343e73fc73acdc4b18b56c7a2cc9f99f3d5b2590364aa8dd386308e5759e691c
overlay     9b5bdd74a039f3a241ce5461ee7fa024ad1601c86e5c709b2811918acbc82705
```

The compatible raw DOT for the migrated input was:

```text
d5c791e913669c4c7d8945780bce290673f92a8ce6a48a2f433e14400d452ef4
```

Graphviz 2.43.0 accepted raw and all six semantic outputs. Generated DOT and PNG
review files were kept in the ignored `build/` directory. The existing tracked
raw SVG at
`/home/katsumata-m/ops/src/ltfa/ebino/main.svg` was refreshed from the corrected
and migrated model. The accepted compact protection SVG was stored at
`/home/katsumata-m/ops/src/ltfa/ebino/protection.svg` with SHA-256:

```text
516ee26db3a4bedd81c2156cc14bdbe7b8611be86cda54133bad00d87fc165e8
```

## Structural reduction

The source model contains 105 entities, 52 endpoints, and 206 model relations.
It also declares five link-down scenarios. The raw presentation contains 157
nodes and 258 edges: 206 model relations
plus 52 synthetic ownership edges.

| View | Container clusters | Endpoint ports | Rendered edges | Edge reduction from raw |
| --- | ---: | ---: | ---: | ---: |
| `topology` | 13 | 52 | 47 | 81.8% |
| `logical` | 0 | 0 | 0 | 100.0% |
| `protection` | 1 | 10 | 10 | 96.1% |
| `power` | 13 | 52 | 44 | 82.9% |
| `dependency` | 13 | 52 | 35 | 86.4% |
| `overlay` | 13 | 52 | 10 | 96.1% |

All 70 selected `contained_in` relations were represented by cluster
membership with zero containment fallback edges. All 52 endpoint ownership
facts were represented by ports with zero synthetic `owns` edges in the
spatial semantic views. The protection view folds all ten eligible
`member_of` relations into one control-domain cluster.

## Visual review

The initial semantic topology layout using Graphviz's default top-to-bottom
ranking was 17,205 by 843 pixels and remained impractically wide. The accepted
semantic output records `rankdir=LR`, causing unconnected siblings to stack
while the facility ring remains traceable. The substation 5-to-substation 1
return edge is visible as the closing path. Endpoints without a model label
show only their final ID component, while the complete stable ID remains in
the tooltip.

On this host the migrated topology PNG was 1,182 by 5,424 pixels. It presents
the extra-high-voltage station and substations 1 through 5 as nested facility
and panel clusters in a vertical sequence.

The protection PNG was 623 by 767 pixels. It contains only the five RingSWs,
their ten endpoint ports, and the five link objects inside the dashed
`protocol=other` control-domain cluster. The cycle is visible without
containment, power, VPN, serial, or dependency edges. The logical view is empty
because this model declares no aggregate; this distinguishes ring protection
from link aggregation instead of drawing every relation into one graph.

This rendering acceptance confirms the documented five-link cycle and its
presentation only. A same-segment endpoint analysis traverses
endpoint—link—endpoint. A cross-switch-port analysis remains unreachable
because endpoint ownership is not a traversal relation and the model does not
declare RingSW internal forwarding. Applying the declared segment failure marks
that link immediately unavailable in propagation; path output remains the
separate nominal structural result. End-to-end failover and RingSW SPOF
conclusions therefore remain outside this acceptance.

Fixed positions, `pos`/`pin`, `neato -n2`, interactive HTML, and a new layout
dependency remain deferred.

## Validation matrix

```text
Release  30/30 passed
```

The Release suite includes installed-tree smoke, static offline-boundary
checks, three Linux runtime offline checks, raw Graphviz rendering, and all six
semantic Graphviz views. Focused tests also cover raw byte compatibility,
declaration-order independence, restricted-output omission, escaping,
highlighting, multi-parent/cyclic/cross-scope containment fallback, endpoint
ownership fallback, exact CLI help, invalid values, and the DOT-only option
dependency.
