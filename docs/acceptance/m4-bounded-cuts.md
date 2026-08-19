# M4 bounded service-cut acceptance

Date: 2026-08-19

## Scope

This checkpoint implements the `M4_BOUNDED_CUTS` core-library boundary. It adds deterministic
object and relation cuts of size one and two, common failure-domain scenarios, node-splitting and
relation-capacity bounded max-flow, cumulative work budgets, and structured incomplete results.

The implementation is a source-tree C++ API in `include/ltfa/cut_analysis.hpp`. It does not extend
the CLI contract, execute a probe, access a network, or claim that a modeled cut is a live failure.
The requirements and basic-design documents referenced by the residual PERT plan are not present
in this checkout; the executable acceptance basis is therefore the PERT task text, the established
Slice 2 analysis semantics, and the tests below.

## Accepted behavior

- A reachable two-path diamond reports minimum object and relation cuts of two.
- Exact two-object and two-relation cut findings are deterministic.
- A shared failure domain is reported as one common-cause scenario with expanded members.
- Source and target objects cannot be selected as cut elements.
- `max_cut_size` is restricted to the planned range one through two.
- Candidate-set, traversal, relation, result, and flow-transition budgets are cumulative.
- Any exhausted work budget returns `AnalysisStatus::incomplete`, not a partial complete claim.
- Repeated input and budgets produce stable findings and usage counters.

## Evidence

```text
cmake --preset release
cmake --build --preset release
ctest --test-dir build/release --output-on-failure
43/43 tests passed

cmake -S . -B build/ubsan -DCMAKE_BUILD_TYPE=Debug -DLTFA_SANITIZERS=undefined
cmake --build build/ubsan --target ltfa_cut_analysis_test
ctest --test-dir build/ubsan -R ltfa.analysis.service-cuts --output-on-failure
1/1 test passed
```

`M4_NEXT_OBSERVATION`, the combined Slice 4 acceptance gate, benchmark work, CLI exposure, release,
and publication remain outside this checkpoint.
