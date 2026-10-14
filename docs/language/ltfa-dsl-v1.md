# LTFA DSL v1 Reference

LTFA DSL v1 is the line-oriented local description language accepted by the LTFA model compiler.
Use `ltfa guide` for bundled focused guidance and this document for the complete user-facing
reference. The parser, not this prose, remains the final authority when an input is rejected.

## Document form

Every source, including every included source, begins with the exact header:

```text
ltfa 1
```

Blank lines are ignored. A `#` outside a quoted string begins a comment through the end of the
line. Sources must be valid UTF-8 and must not begin with a UTF-8 byte-order mark. A quoted string
must terminate on the same line; backslash escape sequences are not interpreted.

The default bounds are:

| Boundary | Limit |
| --- | ---: |
| One source | 16 MiB |
| Complete source tree | 64 MiB |
| One line | 8 KiB |
| Include depth | 16 |
| Attributes on one statement | 64 |
| One attribute string | 4 KiB |

## Tokens and values

Stable identifiers match `[a-z0-9][a-z0-9._:-]*`. Attribute keys match
`[a-z][a-z0-9-]*`; unknown attributes are accepted only when the key begins with `x-`.

Attribute values use one of these forms:

- identifier: the stable-ID grammar above
- string: one line of UTF-8 text between double quotes
- integer: optional `-` followed by decimal digits
- real: optional `-`, decimal digits, one `.`, and decimal digits
- boolean: `true` or `false`
- duration: decimal digits followed by `ms`, `s`, `m`, `h`, or `d`

Attributes use `key=value`. They cannot repeat, except that `failure-domain` may repeat on an
`entity` or `endpoint`.

## Statements

### Scope

```text
scope <id> [kind=<scope-kind>] [label=<value>]
  [completeness=verified|partial|unknown]
  [evidence-quality=confirmed|inferred|unknown]
```

Supported scope kinds are `site`, `cloud`, `shared`, and `other`.

### Entity

```text
entity <id> [kind=<entity-kind>] [plane=<plane>] [layer=<layer>]
  [scope=<scope-id>] [label=<value>] [network-space=<entity-id>]
  [failure-domain=<id>]... [sensitivity=internal|restricted]
  [source-ref=<value>] [address=<value>] [medium=<value>]
  [protocol=<value>] [mode=<value>] [role=<value>] [x-<key>=<value>]...
```

Entity kinds are `device`, `facility`, `bus`, `link`, `aggregate`, `control-domain`,
`network-space`, `tunnel`, `service`, `process`, and `other`.

Planes are `facility`, `communication`, `control-signal`, and `service`. Layers are `none`,
`physical`, `l2`, `l3`, `overlay`, `virtual-network`, `transport`, and `application`.

Aggregate and control-domain entities have additional validated `protocol`, `mode`, and relation
metadata rules. Use `ltfa guide statements relations --level detail` and validate the complete
model before relying on aggregate availability.

Turbo Ring forwarding uses the existing `control-domain protocol=turbo-ring` and membership
`normal-state` fields. Physical availability and logical blocking are distinct; see
[the forwarding contract](../turbo-ring-forwarding.md) for complete topology and policy inputs.

### Endpoint

```text
endpoint <id> owner=<entity-id> [kind=<endpoint-kind>] [scope=<scope-id>]
  [label=<value>] [network-space=<entity-id>] [failure-domain=<id>]...
  [sensitivity=internal|restricted] [source-ref=<value>] [address=<value>]
  [medium=<value>] [protocol=<value>] [role=<value>]
  [normal-state=<value>] [current-range=<value>] [x-<key>=<value>]...
```

Endpoint kinds are `network`, `power-input`, `power-output`, `serial`, `contact-input`,
`contact-output`, `analog-current-input`, `analog-current-output`, and `logical`.

### Connections and relations

```text
connect <from-object-id> -- <to-object-id> kind=<relation-kind> [attributes]
connect <from-object-id> -> <to-object-id> kind=<relation-kind> [attributes]
relate <from-object-id> -> <to-object-id> kind=<relation-kind> [attributes]
```

`--` is bidirectional and `->` is directed. `relate` accepts only `->`. Supported relation
attributes are `kind`, `label`, `scope`, `sensitivity`, `source-ref`, `role`, `normal-state`, and
`x-` extensions.

Built-in relation kinds are `network`, `power`, `serial`, `contact`, `analog-current`,
`member_of`, `contained_in`, `carried_by`, `uses_underlay`, `terminates_on`, `routes_via`,
`depends_on`, and `powered_by`.

### Availability requirements

```text
require <consumer-entity-id> by <provider-entity-id>
  group=<id> mode=all|any [hold-up=<duration>] [x-<key>=<value>]...

require <consumer-entity-id> by <provider-entity-id>
  group=<id> mode=at-least minimum=<positive-integer>
  [hold-up=<duration>] [x-<key>=<value>]...
```

All declarations with the same consumer and `group` form one requirement group. `minimum` is
required for `at-least` and is rejected for the other modes.

### Failure

```text
failure <id> target=<object-id> mode=<failure-mode>
  [after=<duration>] [x-<key>=<value>]...
```

Built-in failure modes include power, link, port, device, route, bus, contact, analog-current,
tunnel, peer, security-association, heartbeat, and probe-path failures. A mode beginning with
`x-` is retained as an explicit extension. `after` models a fixed propagation delay; it does not
schedule or observe a live failure.

### Probe

```text
probe <id> target=<object-id> vantage=<entity-id>
  kind=icmp|tcp-connect|custom-heartbeat|metric|discrete-state
  valid-for=<duration> [x-<key>=<value>]...
```

A probe statement registers an evidence source for offline diagnosis. LTFA never executes the
probe or contacts its producer.

### Include

```text
include "relative/path.ltfa"
```

The path must be quoted and relative. The canonical target must be a regular file below the
explicit `--model-root`. Absolute paths, URI schemes, escapes, cycles, duplicate canonical
sources, excessive depth, and configured size-limit violations are rejected.

## Minimal model

```text
ltfa 1
scope site:example kind=site completeness=verified evidence-quality=confirmed
entity service:app kind=service plane=service layer=application scope=site:example
entity process:worker kind=process plane=service layer=application scope=site:example
relate service:app -> process:worker kind=depends_on
```

The installed complete sample is `share/doc/ltfa/examples/minimal/main.ltfa`. The source-tree
sample is `examples/minimal/main.ltfa`.

Validate it with:

```sh
ltfa model --model-root examples/minimal --input main.ltfa
```

## Diagnostic navigation

- `E_DSL_*`: `ltfa guide errors dsl`
- `E_INCLUDE_*`: `ltfa guide errors include`
- normalized-model validation `E_*`: `ltfa guide errors validation`

Diagnostics identify the first rejected location. Correct the local input and rerun the selected
read-only command. A diagnostic or guide route never applies a fix automatically.
