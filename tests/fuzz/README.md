# Parser and observation fuzzing

The `ltfa_parser_fuzz` target feeds bounded byte strings through topology
parsing, normalization, and deterministic JSON/DOT rendering. The
`ltfa_observation_fuzz` target feeds bounded byte strings through strict JSON
Lines decoding. Replay both synthetic corpora with any supported compiler:

```bash
cmake --preset fuzz-seeds
cmake --build --preset fuzz-seeds
ctest --preset fuzz-seeds
```

For coverage-guided fuzzing, configure with Clang and libFuzzer:

```bash
cmake -S . -B build/fuzz \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DLTFA_BUILD_FUZZERS=ON \
  -DLTFA_FUZZ_ENGINE=libfuzzer \
  -DLTFA_SANITIZERS='address;undefined'
cmake --build build/fuzz --target ltfa_parser_fuzz ltfa_observation_fuzz
./build/fuzz/tests/ltfa_parser_fuzz tests/fuzz/corpus/parser -max_len=1048576
./build/fuzz/tests/ltfa_observation_fuzz tests/fuzz/corpus/observation -max_len=1048576
```

Keep corpus inputs synthetic and free of credentials or unrestricted evidence.
