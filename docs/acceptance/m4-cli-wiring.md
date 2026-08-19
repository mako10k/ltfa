# Slice 4 CLI wiring implementation checkpoint

Date: 2026-08-19

The implementation adds `cuts` and `diagnose` to CLI Contract 2 discovery. Acceptance evidence
must cover structured help, human and JSON results, diagnosis DOT, bounded incomplete results,
usage/input/validation statuses, canonical observation-root confinement, installed execution,
zero network syscalls, and the complete regression suite.

The exact command and schema design is recorded in ADR 0012 with owner acceptance still pending.
This checkpoint therefore records implemented and tested behavior, not an accepted architecture
decision.

## Evidence

```text
cmake --preset release
cmake --build --preset release
ctest --test-dir build/release --output-on-failure
49/49 tests passed

cmake -S . -B build/ubsan -DCMAKE_BUILD_TYPE=Debug -DLTFA_SANITIZERS=undefined
cmake --build build/ubsan --target ltfa
ctest --test-dir build/ubsan -R ltfa.cli.slice4-acceptance --output-on-failure
1/1 test passed
```

The Release matrix includes installed `cuts` and `diagnose`, structured complete and incomplete
JSON, diagnosis DOT, canonical observation-root rejection, structured usage errors, and a dedicated
zero-network-syscall trace for both new commands.
