# Slice 4 CLI wiring implementation checkpoint

Date: 2026-08-19

Checkpoint status: implemented and tested

Post-checkpoint disposition: accepted by final MVP owner acceptance on 2026-08-24

The implementation adds `cuts` and `diagnose` to CLI Contract 2 discovery. Acceptance evidence
must cover structured help, human and JSON results, diagnosis DOT, bounded incomplete results,
usage/input/validation statuses, canonical observation-root confinement, installed execution,
zero network syscalls, and the complete regression suite.

At this checkpoint, the exact command and schema design in ADR 0012 still required owner
acceptance. Final MVP receipt `RCPT_MVP_OWNER_ACCEPTED_20260824` subsequently satisfied that gate in
commit `bf053b17f7a0ba85b241fa48a63b8ca2ec6bf8a1`; the task completed in
`d18cafdf014acae4a930af5b4fb526c10bcef965` and the plan reached `MVP_ACCEPTED` in
`85faabf1129a7fb3dd44b3e451ba705395f6f6d1`. This record remains the historical implementation
checkpoint, while ADR 0012 now records the accepted architecture disposition.

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
