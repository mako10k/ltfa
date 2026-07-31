# M3 Redacted Historical Fixture Acceptance

Date: 2026-07-31

Task: `M3_REDACTED_AMI_FIXTURE`

Status: accepted

## Delivered contract

The offline integration fixture is a synthetic derivative of the reviewed
Ami-A historical structure. It preserves only cardinalities, target-equivalence
classes, observation-vantage/history concepts, and explicit uncertainty. The
transformation and exclusions are recorded in
[ADR 0010](../decisions/0010-redacted-historical-fixture.md).

The model contains two partial/inferred scopes, twelve synthetic entities, nine
dependencies, four declared failure modes, and four registered ICMP probes.
Five synthetic collector services retain only the approved fanout values and
depend on three synthetic target classes. No target address or operational
identifier is present.

Six synthetic JSON Lines observations produce four evidence groups, one
same-probe conflict, and one cross-vantage conflict. Diagnosis completes with a
ten-candidate set. The distinct rank-one candidate is the declared synthetic
remote path loss: it explains the remote timeout without contradicting the
current local pass. The result still reports one unexplained non-pass, four
conflicting evidence records, two remaining unknown evidence records, and
`partial`/`inferred` qualification. It is a hypothesis result, not a confirmed
historical diagnosis.

## Requirement evidence

| Requirement | Evidence and boundary |
| --- | --- |
| `AC-006`, `REQ-OBS-008` | The fixture retains both current local pass and remote timeout plus a same-probe disagreement; the evidence index returns one conflict of each kind and diagnosis retains all four conflicting latest records. |
| `AC-007`, `REQ-ANL-015` | Repeated diagnosis returns identical ordered stable IDs, ranks, ties, scores, qualifications, causal usage, candidate count, simulation count, and evidence-comparison count. |
| `AC-008`, `REQ-SAF-001..002` | The integration test consumes only committed local files, requires no environment value or secret, passes the static boundary suite, and has a dedicated zero-network-syscall `strace` test. |
| `AC-009`, `REV-009` | The fixture retains five-to-three mapping structure, fanouts, six-record history, and two vantage roles while omitting operational values and unrestricted raw evidence. It runs without the adjacent operations repository. |
| `REQ-ANL-010..014`, `AC-005` | The candidate set stays inside observed causal cones, ranks a vantage path hypothesis above contradicted target loss, retains unknowns, exposes no probability, and keeps partial/inferred qualification. |

## Redaction evidence

The committed test rejects operational and secret-shaped names, IPv4 and email
patterns, and operational absolute paths. Every decoded record must omit
`evidence_ref`, `detail`, producer ID/sequence, and extensions.

A local structure-derivation check compared the model and observation payload
with all 106 source-snapshot string values of length four or more. Exact source
value substring collisions: `0`. Only that aggregate result is retained here;
the values themselves were neither printed into the fixture nor committed.

Explicitly excluded categories are site/access values, target and host
addresses, converter/device/database identifiers, alarm readings and prose,
original timestamps and summaries, office allowlists, issue/journal/source
references, credentials, and unrestricted evidence.

## Focused coverage

- fixture-root loading and deterministic normalized compilation
- exact two-scope, twelve-entity, nine-dependency, four-failure, four-probe shape
- five-to-three collector target structure and fanout values
- six strict synthetic observations with no raw-evidence fields
- current/stale history separation
- same-probe and cross-vantage conflict retention
- explainable ten-candidate diagnosis and distinct rank-one path hypothesis
- partial/inferred qualification and remaining unknown evidence
- repeatable ordering, ranks, score facts, and usage
- exclusion of downstream collector inventory from the causal cone
- committed payload leak patterns and zero runtime network syscalls

## Validation

The complete validation matrix passes:

```text
Release                                  39/39 passed
ASan + UBSan                             32/32 passed
LeakSanitizer                            32/32 passed
ASan + UBSan standalone fuzz-seed replay 34/34 passed
Clang 18 ASan + UBSan                    32/32 passed
Clang 18 libFuzzer core and targets      built
Source scalar collision check            0/106 collisions
```

Release includes the fixture-specific runtime `strace`, static offline
boundary, existing CLI/core runtime checks, installed-tree smoke, and all
available Graphviz checks. The source snapshot is not a runtime or test
dependency, and the CLI contract remains unchanged. PERT completion and
advancement remain separate mutations after this implementation and evidence
are committed.
