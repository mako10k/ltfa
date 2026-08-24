# ADR 0013: Two-tier MVP benchmark acceptance

- Status: Accepted
- Date: 2026-08-24
- Decision Owner: user
- Related Claims: `C-BENCH-POLICY-001`, `C-BENCH-MEM-001`
- Related Evidence: `E-BENCH-ACC-001..007`, `E-BENCH-POLICY-001..002`
- Related Reviews: `docs/acceptance/mvp-benchmark-acceptance.md`
- Supersedes: None
- Superseded by: None

## Context

The pinned five-run benchmark passes the provisional x86_64 elapsed-time and binary-size targets,
but peaks at 96,660 KiB for model compilation and 273,416 KiB for observation processing. Massif
attributes the peaks to simultaneously live full-document representations rather than unstable
measurement noise. The Raspberry Pi 4 produces the same fixture hashes and semantic counts and
passes the Release suite without thermal throttling, but cannot meet the x86_64 elapsed-time limits.

The original 64/96-MiB memory values were provisional. Treating them as immediate MVP gates would
mix a useful long-term footprint objective with prototype acceptance, while applying the x86_64
time limits to the portability host would mix performance comparison with functional portability.
On 2026-08-24 the owner explicitly approved relaxing those roles and optimizing the items that
still exceed the relaxed gate.

## Evidence chain

- `E-BENCH-POLICY-001` — The formal x86_64 sample records medians of 479,262 us and 1,068,467 us,
  maximum RSS values of 96,660 KiB and 273,416 KiB, and a 1,370,416-byte stripped binary.
- `E-BENCH-POLICY-002` — Source inspection shows that observation processing retains its input
  payload while indexing, keeps the moved-from observation-vector allocation while constructing a
  second full representation, and copies the full decoded vector on the product CLI path.
- `C-BENCH-POLICY-001` 📜✅ — Referencing `E-BENCH-POLICY-001` and the owner decision, a two-tier
  policy separates MVP regression ceilings from footprint stretch targets without weakening input
  limits or correctness checks.
- `C-BENCH-MEM-001` 📜➖ — Referencing `E-BENCH-POLICY-001..002`, total RSS remains above a
  256-MiB MVP ceiling, while model RSS already fits a 96-MiB ceiling; the identified lifetime and
  copy changes are sufficient bounded optimization candidates.

## Decision

The x86_64 reference host has these MVP acceptance ceilings on the pinned profile:

| Measure | MVP ceiling |
| --- | ---: |
| Model internal elapsed | 1,000,000 us |
| Model peak RSS | 98,304 KiB (96 MiB) |
| Observation internal elapsed | 2,000,000 us |
| Total peak RSS | 262,144 KiB (256 MiB) |
| Stripped binary | 8,388,608 bytes (8 MiB) |

The original 64-MiB model and 96-MiB total RSS values remain non-blocking stretch targets. Missing a
stretch target must remain visible in the acceptance report but does not by itself reject the MVP.

The aarch64 Raspberry Pi 4 is a required portability gate, not a second numeric performance gate.
It must use identical fixture bytes, produce identical semantic counts, pass the complete Release
suite, and show no thermal throttling during the accepted sample. Its elapsed time, peak RSS, binary
size, temperature, and governor are still recorded and qualified.

No parser limit, observation limit, validation, fail-closed behavior, sanitizer requirement, or
offline boundary is relaxed. The measured product path must move decoded observations into the
index and release input storage once decoding has completed.

## Alternatives

- Keep 64/96 MiB as hard MVP gates: rejected because the allocation profile shows those values
  require broader streaming work than the bounded prototype acceptance task.
- Accept the observed 273,416 KiB as the total ceiling: rejected because it would remove the
  optimization pressure established by the evidence.
- Apply x86_64 time limits to every architecture: rejected because it confuses portability with a
  hardware performance comparison.

## Consequences

Good:

- MVP acceptance retains measurable regression ceilings and still requires a memory improvement.
- The original footprint ambition remains explicit and can guide later streaming work.
- aarch64 correctness remains mandatory without pretending different hardware is performance
  equivalent.

Bad / Risk:

- The accepted MVP may remain above the stretch memory footprint.
- The 96-MiB model ceiling has little margin over the current sample and must be rechecked on every
  accepted run.

Neutral:

- Benchmark fixtures, host inventory, run count, phase boundaries, and all functional limits stay
  unchanged.

## Implementation Notes

- `A-BENCH-POLICY-001` 🚀 [executed] — Record this accepted policy in the benchmark protocol and
  acceptance evidence. References `C-BENCH-POLICY-001`.
- `A-BENCH-MEM-001` 🚀 [executed] — Released moved-from/input storage, eliminated the product CLI
  observation copy, and repeated the pinned benchmark. x86_64 total RSS fell to 200,264 KiB and the
  full product CLI stayed at 233,944 KiB. References `C-BENCH-MEM-001`.
- Do not optimize the model parser solely to meet the non-blocking 64-MiB stretch target in this
  task.

## Review

- Specialist review: allocation profile and ownership lifetimes reviewed against
  `src/ltfa/evidence.cpp`, `app/cli.cpp`, and the benchmark driver.
- Non-specialist review: owner approved relaxation plus optimization on 2026-08-24.
- Root-chain review: implementation actions reference accepted claims and measured evidence; no
  network, command execution, persistence, plugin, or dependency boundary changes.

## Follow-ups

- Obtain final owner acceptance before finishing the PERT task or reaching `MVP_ACCEPTED`.
- Keep parser streaming and the 64/96-MiB stretch targets as post-MVP work unless later evidence
  promotes them to a separately approved gate.
