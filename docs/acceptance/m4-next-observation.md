# M4 next-useful-observation acceptance

Date: 2026-08-19

## Scope

This checkpoint implements the `M4_NEXT_OBSERVATION` source-core boundary. Given a complete
diagnosis, it examines registered probes without current evidence and recommends the probe that
best partitions each tied hypothesis rank by predicted pass/non-pass outcome.

The API in `include/ltfa/observation_recommendation.hpp` is recommendation-only. It does not run a
probe, contact a producer, access a network, change evidence, or expose a new CLI command.

## Accepted behavior

- Only registered probes lacking current evidence are candidates; stale evidence may be refreshed.
- Recommendations are generated only for real tied diagnosis ranks.
- Predictions use each simulated finding's affected and remaining-unknown objects.
- Unknown predictions do not count as a discriminating pair.
- The best probe maximizes separated hypothesis pairs, then uses stable probe ID order.
- Candidate-probe, hypothesis-comparison, and result budgets are cumulative.
- Budget exhaustion and incomplete diagnosis return structured incomplete results without a
  complete recommendation claim.
- Invalid model or evidence references fail closed.
- Repeated semantic input produces identical recommendations and usage counters.

## Evidence

```text
cmake --preset release
cmake --build --preset release
ctest --test-dir build/release --output-on-failure

cmake -S . -B build/ubsan -DCMAKE_BUILD_TYPE=Debug -DLTFA_SANITIZERS=undefined
cmake --build build/ubsan --target ltfa_observation_recommendation_test
ctest --test-dir build/ubsan -R ltfa.analysis.next-observation --output-on-failure
```

Slice 4 combined acceptance, CLI exposure, live observation execution, benchmark work, release,
and publication remain separate.
