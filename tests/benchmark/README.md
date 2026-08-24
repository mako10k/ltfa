# LTFA MVP benchmark profile

This directory contains the non-installed fixture generator and measurement driver for the
governed MVP and stretch performance targets. They are not part of the normal CTest suite and do
not change the one-shot offline product boundary.

## Pinned profile

Profile `mvp-10k-50k-100k-v1` contains:

- 10,000 device entities in one verified scope;
- 50,000 acyclic `depends_on` relations;
- one registered ICMP probe; and
- 100,000 deterministic historical observation records for that probe.

The generated files are intentionally excluded from the repository. Generate them below `build/`:

```sh
python3 tests/benchmark/generate_fixtures.py \
    --output-root build/benchmark-fixtures
```

The pinned generator and output identities are:

| Artifact | SHA-256 | Bytes |
| --- | --- | ---: |
| `generate_fixtures.py` | `b61d0a6c168636a7ccefc3896052a754dc53c523de658cdc46b664eaa6863a56` | repository source |
| `main.ltfa` | `cae10e1791a38371d1cd1cdebcc6e16782d73e31d855ab86bb6bb9d4735a26a7` | 4,030,216 |
| `observations.jsonl` | `bb193e68d7df67e133cf521e07f5592d23db689e969ffaf0b39149b619846c61` | 23,588,890 |

`manifest.json` records the same identities, semantic counts, and the fixed analysis reference time
`2030-01-02T03:47:00Z`. Identical bytes were generated with Python 3.12.3 on x86_64 and Python 3.11.2
on aarch64.

## Build

Use the committed Release profile. The driver is excluded from the default build and is never
installed.

```sh
cmake --preset release
cmake --build --preset release --target ltfa ltfa_benchmark_driver
```

## Measurement protocol

Record the source commit, host inventory, compiler and CMake versions, load average, and available
memory. On Raspberry Pi, also record temperature, scaling governor, and `vcgencmd get_throttled`
before and after measurement.

Use one discarded warm-up followed by five measured runs. Do not drop filesystem caches. Pin the
single-threaded process to logical CPU 0 with `taskset -c 0`. Report every run, the median internal
elapsed time, and the maximum peak RSS. The observation driver's internal `elapsed_us` starts after
model compilation and covers bounded file loading, JSON Lines decoding, freshness classification,
and evidence indexing. External peak RSS covers the complete process, including the compiled model.

Warm-up:

```sh
taskset -c 0 ./build/release/tests/ltfa_benchmark_driver \
    model build/benchmark-fixtures main.ltfa >/dev/null
taskset -c 0 ./build/release/tests/ltfa_benchmark_driver \
    observations build/benchmark-fixtures main.ltfa observations.jsonl \
    2030-01-02T03:47:00Z >/dev/null
```

Measured runs:

```sh
for run in 1 2 3 4 5; do
    /usr/bin/time -f "run=$run phase=model wall_s=%e rss_kb=%M" \
        taskset -c 0 ./build/release/tests/ltfa_benchmark_driver \
        model build/benchmark-fixtures main.ltfa
done 2>&1 | tee build/benchmark-model-results.txt

for run in 1 2 3 4 5; do
    /usr/bin/time -f "run=$run phase=observations wall_s=%e rss_kb=%M" \
        taskset -c 0 ./build/release/tests/ltfa_benchmark_driver \
        observations build/benchmark-fixtures main.ltfa observations.jsonl \
        2030-01-02T03:47:00Z
done 2>&1 | tee build/benchmark-observation-results.txt
```

Measure the stripped dynamically linked CLI without altering the validated executable:

```sh
mkdir -p build/benchmark-artifacts
cp build/release/ltfa build/benchmark-artifacts/ltfa.stripped
strip --strip-all build/benchmark-artifacts/ltfa.stripped
stat -c 'stripped_binary_bytes=%s' build/benchmark-artifacts/ltfa.stripped
```

ADR 0013 governs the accepted two-tier targets. On the x86_64 reference host, the MVP ceilings are
model time at most 1 second, model peak RSS at most 96 MiB, observation time at most 2 seconds,
total peak RSS at most 256 MiB, and stripped binary size at most 8 MiB. The original 64-MiB model
and 96-MiB total RSS values remain recorded as non-blocking stretch targets.

The aarch64 host is a required portability gate: fixture hashes and semantic counts must match,
the Release suite must pass, and the accepted sample must not be thermally throttled. Its numeric
performance is recorded and qualified but is not compared to the x86_64 ceilings. A miss never
authorizes removal of validation or resource limits.
