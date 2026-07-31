# ADR 0007: Strict Observation JSON Lines Contract

Date: 2026-07-31

Status: Accepted

## Context

The reviewed MVP requires bounded UTF-8 observation records without adding a
monitoring runtime to LTFA. Every record must retain probe, target, vantage,
kind, outcome, observation time, and validity. Unsupported future schema
majors must fail explicitly, malformed input must expose no partial batch, and
the core must not open a network connection, launch a producer, load a plugin,
or read an implicit clock.

The adjacent basic design names the required and optional fields but leaves
the exact C++ value types, extension subset, and decoder-versus-evidence-index
boundary to implementation.

## Decision

`decode_observation_json_lines` is a public, value-oriented core API. It
receives one already loaded `std::string_view`, a source ID, and explicit
limits. It performs no file access. Empty input represents zero observations;
otherwise every physical line, excluding one final line delimiter, must hold
exactly one JSON object. CRLF and LF are accepted. Blank interior lines,
multiple JSON values on one line, JSON5, comments, trailing commas, invalid
UTF-8, infinity, and NaN fail.

The only supported schema is `ltfa.observation/v1`. Required fields are:

- `schema`
- `probe_id`
- `target`
- `vantage`
- `kind`
- `outcome`
- `observed_at`
- `valid_for_ms`

Optional built-in fields are `duration_ms`, `value`, `unit`, `state`,
`producer_sequence`, `producer_id`, `evidence_ref`, and `detail`. IDs use the
same lowercase stable-ID grammar as the topology model. Durations, validity,
and producer sequence are unsigned integers. A metric `value` retains signed
integer, unsigned integer, or finite real representation and must appear with
`unit`; both are limited to `kind=metric`. `state` is limited to
`kind=discrete-state`.

Kinds are exactly `icmp`, `tcp-connect`, `custom-heartbeat`, `metric`, and
`discrete-state`. Outcomes are exactly `pass`, `fail`, `timeout`, `error`, and
`unknown`. The decoder preserves these meanings without inferring target
availability. In particular, an ICMP timeout remains a typed observation, not
a device-down or power-loss result.

`observed_at` is an RFC 3339 profile with a four-digit nonzero Gregorian year,
valid calendar date, required `T`/`t`, seconds including the RFC leap-second
value 60, up to nine fractional digits, and `Z`/`z` or a numeric offset.
Unknown-local-offset `-00:00` is rejected because later freshness comparison
requires a known instant. The API retains the source text and decoded calendar,
fraction, and offset components; it does not consult the system clock.

Unknown built-in-looking fields fail. Extension keys must use a lowercase
`x-` name and their values are limited to null, boolean, finite number, or a
bounded string. Arrays and objects are rejected so raw response bodies cannot
be tunneled through an extension. Extension keys containing common
secret-bearing names such as password, credential, token, PSK, private key, or
API key are rejected. This structural check supplements but cannot replace the
producer's obligation to redact values before they cross the protocol
boundary.

Default limits are 64 MiB for the complete input, 64 KiB for one record,
100,000 records, 16 extensions per record, 64 bytes per extension key, and
4 KiB per decoded string. Limit changes are caller policy, never input data.
On any JSON, schema, semantic, or limit error, the result contains one stable
diagnostic and no observations.

Probe registration, target/vantage reference resolution, custom-heartbeat
mapping, freshness against an explicit analysis time, and conflict handling
belong to the following evidence-index task. CLI file/stdin loading and
diagnosis output are also outside this decoder slice.

## Consequences

The decoder can be fuzzed as a pure bounded function and yyjson remains behind
the public LTFA value types. Future schema support requires a reviewed version
change instead of silently accepting new fields. The CLI still has no
observation option, and this decision does not authorize or implement active
probes, monitoring adapters, network access, command execution, plugins,
databases, daemons, or secret retrieval.
