# MVP benchmark acceptance measurement

Date: 2026-08-24

Task: `MVP_BENCHMARK_ACCEPTANCE`

Status: measurement complete and source snapshot sealed; qualified target disposition and final
owner acceptance remain open

## Claim

`C-BENCH-ACC-001` 📜✅ — The pinned five-run protocol completed on the x86_64 reference host and
the aarch64 portability host with identical fixture hashes and semantic counts. The x86_64 result
meets the provisional model-time, observation-time, and stripped-binary targets, but misses both RSS
targets. The aarch64 result demonstrates functional portability but misses the provisional time and
RSS targets. These results do not yet justify `MVP_ACCEPTED`.

## Evidence

- `E-BENCH-ACC-001` — Both hosts used model SHA-256
  `cae10e1791a38371d1cd1cdebcc6e16782d73e31d855ab86bb6bb9d4735a26a7` and observation SHA-256
  `bb193e68d7df67e133cf521e07f5592d23db689e969ffaf0b39149b619846c61`.
- `E-BENCH-ACC-002` — Every measured model run reported 10,000 entities, 50,000 relations, and one
  probe. Every measured observation run reported 100,000 observations, 40 current, 99,960 stale,
  one group, and zero conflicts.
- `E-BENCH-ACC-003` — Release CTest passed 49/49 on both hosts after measurement.
- `E-BENCH-ACC-004` — The Raspberry Pi reported `schedutil`, 66.7°C before and 67.6°C after the
  sample, with `get_throttled=0x0` both before and after.
- `E-BENCH-ACC-005` — At measurement time, the source basis was commit
  `640c2d133e013180cb90c3960345cbc5e1df80a7` plus the then-uncommitted benchmark and documentation
  changes. The copied aarch64 tree has no Git metadata, so its correspondence is established by the
  copied source tree, identical fixtures, build output, and semantic readback rather than a remote
  Git revision readback.
- `E-BENCH-ACC-006` — x86_64 Valgrind Massif profiling attributed the model peak to 94,700,325
  useful heap bytes plus 4,392,587 allocator-overhead bytes. Token storage (44,802,720 bytes), parsed
  statement storage (30,670,848 bytes), and parsed attributes (10,043,520 bytes) account for 90.3%
  of the useful peak.
- `E-BENCH-ACC-007` — The observation peak contained 229,094,227 useful heap bytes plus 7,838,413
  allocator-overhead bytes. The decoded observation vector (67,108,864 bytes), resolved-observation
  workspace (53,600,000 bytes), indexed-observation vector (52,800,000 bytes), and retained input
  payload (23,588,891 bytes) account for 86.0% of the useful peak.
- `E-BENCH-ACC-008` — Commit `823dc9926b09ac70bd7b9817094415ef3fb825c4` seals the generator,
  benchmark driver, protocol, result report, raw-evidence hashes, and pre-advance PERT/history state.

## Five-run results

The internal elapsed time follows the phase boundaries in `tests/benchmark/README.md`. Wall time and
peak RSS are from GNU `time`; observation wall time and RSS cover the complete process, including
model compilation.

### x86_64 reference host

| Run | Model internal (us) | Model wall (s) | Model RSS (KiB) | Observation internal (us) | Total wall (s) | Total RSS (KiB) |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 384,044 | 0.39 | 96,660 | 1,031,144 | 1.61 | 273,404 |
| 2 | 406,016 | 0.41 | 96,608 | 1,317,678 | 2.29 | 273,352 |
| 3 | 515,091 | 0.52 | 96,604 | 1,366,803 | 2.37 | 273,388 |
| 4 | 479,262 | 0.49 | 96,644 | 1,068,467 | 2.33 | 273,416 |
| 5 | 507,665 | 0.52 | 96,564 | 974,516 | 1.84 | 273,384 |
| **Median / maximum** | **479,262** | — | **96,660** | **1,068,467** | — | **273,416** |

The live pre-sample load average was 6.96/5.50/4.08 with 20,609,028 KiB available memory. The
stripped dynamically linked binary was 1,370,416 bytes.

### aarch64 portability host

| Run | Model internal (us) | Model wall (s) | Model RSS (KiB) | Observation internal (us) | Total wall (s) | Total RSS (KiB) |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 3,918,587 | 3.98 | 95,540 | 3,272,120 | 7.51 | 272,308 |
| 2 | 3,926,332 | 3.99 | 95,540 | 3,272,471 | 7.44 | 272,296 |
| 3 | 3,926,803 | 3.99 | 95,540 | 3,235,686 | 7.34 | 272,308 |
| 4 | 3,996,110 | 4.06 | 95,540 | 3,256,369 | 7.35 | 272,304 |
| 5 | 3,971,036 | 4.04 | 95,540 | 3,290,937 | 7.44 | 272,308 |
| **Median / maximum** | **3,926,803** | — | **95,540** | **3,272,120** | — | **272,308** |

The live pre-sample load average was 0.46/0.39/0.76 with 2,706,604 KiB available memory. The
stripped dynamically linked binary was 1,119,760 bytes.

## Target evaluation

| Target | Provisional limit | x86_64 result | x86_64 status | aarch64 result | aarch64 qualification |
| --- | ---: | ---: | --- | ---: | --- |
| Model internal elapsed | ≤ 1,000,000 us | 479,262 us | pass | 3,926,803 us | miss |
| Model peak RSS | ≤ 65,536 KiB | 96,660 KiB | miss | 95,540 KiB | miss |
| Observation internal elapsed | ≤ 2,000,000 us | 1,068,467 us | pass | 3,272,120 us | miss |
| Total peak RSS | ≤ 98,304 KiB | 273,416 KiB | miss | 272,308 KiB | miss |
| Stripped binary | ≤ 8,388,608 bytes | 1,370,416 bytes | pass | 1,119,760 bytes | pass |
| Release suite | 49/49 | 49/49 | pass | 49/49 | pass |

The x86_64 model RSS exceeds the provisional limit by 31,124 KiB (47.5%), and total RSS exceeds it
by 175,112 KiB (178.1%). The near-identical RSS plateaus across runs and architectures make random
noise an unlikely complete explanation. Massif establishes that the bulk comes from simultaneous
full-document token/AST storage during model parsing and simultaneous payload/decoded/resolved/index
representations during observation processing.

Static inspection also found that the product CLI passes `decoded.observations` to
`build_evidence_index` as an lvalue, while the benchmark driver moves it. The CLI can therefore add
another full observation-vector copy relative to this benchmark path. A CLI-specific profile is
needed before treating the dedicated driver's RSS as a product-path upper bound.

## Reasoning

- `R-BENCH-ACC-001` 🧠 — Referencing `E-BENCH-ACC-001..004`, identical inputs, semantic counts,
  stable per-host results, and no Pi throttling make this a reproducible portability and measurement
  result.
- `R-BENCH-ACC-002` 🧠 — Referencing the target table, passing time and binary targets cannot
  compensate for two failed RSS targets. The result is qualified evidence, not an unqualified MVP
  acceptance.
- `R-BENCH-ACC-003` 🧠 — Referencing `E-BENCH-ACC-005` and `E-BENCH-ACC-008`, the measured source
  basis is now reviewable from a sealed repository snapshot; final acceptance still depends on the
  governed disposition of the measured target misses.
- `R-BENCH-ACC-004` 🧠 — Referencing `E-BENCH-ACC-006..007`, both RSS misses have identified
  structural high-water causes. Meeting the existing limits requires lifecycle/streaming changes or
  an explicitly governed target revision; measurement noise cannot justify promotion.

## Actions

- `A-BENCH-ACC-001` 🧪 [executed] — Completed the warm-up plus five measured model and observation
  runs on both pinned hosts. References `C-BENCH-ACC-001`.
- `A-BENCH-ACC-002` 🧪 [executed] — Re-ran Release CTest 49/49 on both hosts and recorded binary
  sizes and Pi thermal/throttle state. References `C-BENCH-ACC-001`.
- `A-BENCH-ACC-003` 🧪 [executed] — Collected allocation-level evidence for both RSS misses without
  changing the measured build. References `C-BENCH-ACC-001`.
- `A-BENCH-ACC-004` ⛔ [pending] — Obtain explicit owner acceptance after the target misses are
  optimized or formally qualified; do not finish the PERT task or reach `MVP_ACCEPTED` first.
  References `C-BENCH-ACC-001`.
- `A-BENCH-ACC-005` ⛔ [pending] — Review an optimization design for parser high-water memory,
  observation representation lifetimes, and the CLI copy before implementation or target revision.
  References `C-BENCH-ACC-001`.
- `A-BENCH-ACC-006` 🚀 [executed] — Sealed the reproducible harness, evidence report, and
  pre-advance PERT/history state in commit `823dc9926b09ac70bd7b9817094415ef3fb825c4`.
  References `C-BENCH-ACC-001`.

## Raw evidence

- x86_64: `build/benchmark-model-results.txt` and
  `build/benchmark-observation-results.txt`
- aarch64 copies: `build/benchmark-aarch64-model-results.txt` and
  `build/benchmark-aarch64-observation-results.txt`
- aarch64 source tree and original logs:
  `/tmp/ltfa-aarch64-check-aLg6r7ZK` on `raspi4`
- allocation profiles: `build/massif-model.out` and `build/massif-observations.out`

| Generated evidence | SHA-256 |
| --- | --- |
| x86_64 model log | `c1b31012c8d04f0fe95fd2c8e5ef9b89634ae9d182d070b90c786d15b5719463` |
| x86_64 observation log | `2f1263d8a86229e108af0e880d5d7b5ad8fd71e745b9aa9deaa3f44a271a0717` |
| aarch64 model log | `0521df926204e1516f89f476ed52d0d1e0a6d52003c036623e0298c2641ae190` |
| aarch64 observation log | `5271d4a0a3a2bc62697f57f8af10a7672c7533dfe3d1f656a94291ed96c4018f` |
| model Massif profile | `3e1fbe6f19b74fdc4ca9aad64411c59062316f7adeeee9b804ef2c9a09cbb7c6` |
| observation Massif profile | `ff97fdbee978a813d8fcb0b253c7c9aaa5abef8b8a7bd698720178ca4cec2c3a` |

These generated files are intentionally outside version control. This report preserves their
material values and qualification boundaries.

## Boundary

This measurement does not finish `MVP_BENCHMARK_ACCEPTANCE`, reach `MVP_ACCEPTED`, change a
provisional requirement, publish a release, or authorize any live probe.
