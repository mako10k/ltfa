# LTFA observation JSON Lines v1

`ltfa.observation/v1` is the bounded local evidence format consumed by `ltfa diagnose`. It is
JSON Lines, not LTFA DSL v1. LTFA reads an explicitly selected local file and never obtains or
executes observations itself.

Use `ltfa guide observations` for focused bundled guidance and this manual for the complete
user-facing reference.

## Complete example

Each physical line contains one JSON object:

```json
{"schema":"ltfa.observation/v1","probe_id":"probe:worker","target":"process:worker","vantage":"process:observer","kind":"icmp","outcome":"timeout","observed_at":"2030-01-01T00:10:00Z","valid_for_ms":60000,"duration_ms":1000,"evidence_ref":"synthetic:diagnosis-example"}
```

The installed synthetic example is under `share/doc/ltfa/examples/diagnosis`. In the source tree:

```sh
ltfa model --model-root examples/diagnosis --input main.ltfa
ltfa diagnose --model-root examples/diagnosis --input main.ltfa \
    --observations observations.jsonl --analysis-at 2030-01-01T00:10:30Z --format json
```

## Framing and atomic failure

- Input must be UTF-8.
- Each physical line, excluding one optional final delimiter, contains exactly one JSON object.
- LF and CRLF are accepted.
- Blank interior lines, comments, JSON5, trailing commas, NaN, infinity, arrays as records, and
  multiple values on one line are rejected.
- Any JSON, UTF-8, schema, semantic, or limit error rejects the complete batch. LTFA exposes no
  partial observation result.

The default bounds are 64 MiB for the complete input, 64 KiB for one record, 100,000 records, 16
extensions per record, 64 bytes per extension key, and 4 KiB per decoded string.

## Required fields

Every record contains all eight fields:

| Field | Type and meaning |
| --- | --- |
| `schema` | String, exactly `ltfa.observation/v1` |
| `probe_id` | Stable ID of one registered model probe |
| `target` | Stable ID matching the registered probe target |
| `vantage` | Stable ID matching the registered probe vantage |
| `kind` | One allowed observation kind matching the registered probe |
| `outcome` | One allowed evidence outcome |
| `observed_at` | RFC 3339 timestamp with a known offset |
| `valid_for_ms` | Unsigned validity duration in milliseconds |

Stable IDs use `[a-z0-9][a-z0-9._:-]*`.

## Optional built-in fields

| Field | Type and constraint |
| --- | --- |
| `duration_ms` | Unsigned measured duration |
| `value` | Finite signed integer, unsigned integer, or real; `metric` only |
| `unit` | String required with `value`; `metric` only |
| `state` | String; `discrete-state` only |
| `producer_sequence` | Unsigned producer-defined ordering value |
| `producer_id` | Bounded producer identity string |
| `evidence_ref` | Bounded external evidence-reference string |
| `detail` | Bounded redacted detail string |

Unknown built-in-looking fields fail. Optional fields do not relax the required fields.

## Kinds

- `icmp`
- `tcp-connect`
- `custom-heartbeat`
- `metric`
- `discrete-state`

LTFA preserves the kind and does not execute its corresponding probe.

## Outcomes

- `pass`
- `fail`
- `timeout`
- `error`
- `unknown`

An outcome is qualified evidence, not a confirmed live cause. In particular, `timeout` does not
prove that a device is down, powered off, or disconnected.

## Time and freshness

`observed_at` uses an RFC 3339 profile with a valid Gregorian date, seconds, up to nine fractional
digits, and `Z` or a numeric offset. Unknown-local-offset `-00:00` is rejected.

`ltfa diagnose` requires `--analysis-at`. LTFA compares that explicit reference time with
`observed_at` and `valid_for_ms`; it never reads a system clock. An observation after the reference
time or outside its validity window is not silently treated as current.

## Extensions and redaction

Extensions use lowercase `x-` keys. Values are limited to null, boolean, finite number, or a bounded
string. Arrays and objects are rejected. Keys containing common secret-bearing names such as
password, credential, token, PSK, private key, or API key are rejected.

This structural filter does not replace producer-side redaction. Do not place credentials, raw
responses, private keys, tokens, or unrestricted operational data in observation files.

## Diagnostic navigation

- `LTOBS-001`: correct the contained local observation path.
- `LTOBS-002`: correct the explicit RFC 3339 `--analysis-at` value.
- `LTOBS-003`: correct JSON Lines framing, schema, fields, or bounds.
- `LTOBS-004`: align the record with a registered probe and explicit reference time.

Handled errors provide these ordered next actions:

1. correct the reported local input and rerun `ltfa diagnose`
2. inspect `ltfa guide observations errors`
3. inspect `ltfa help diagnose`

Navigation never edits the file, retries diagnosis, obtains observations, or executes a probe.

## Offline boundary

Observation producers remain separate. LTFA performs no network access, command execution, plugin
loading, database access, environment discovery, secret retrieval, implicit clock read, or daemon
operation. A next-observation recommendation names a registered probe that lacks current evidence;
it does not authorize or execute that probe.

## Related routes

- `ltfa guide observations format`
- `ltfa guide observations fields`
- `ltfa guide observations semantics`
- `ltfa guide observations errors`
- `ltfa guide samples diagnosis`
- `ltfa guide workflows diagnose`
- `ltfa help diagnose`
