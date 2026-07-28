# Repository Guidelines

## Project Structure & Module Organization

LTFA is a bootstrap C++17 project for a lightweight topology and fault analyzer.
Keep public core interfaces in `include/ltfa/` and their implementations in
`src/ltfa/`. The CLI entry point belongs in `app/`. Add CTest targets and test
sources under `tests/`. CMake templates live in `cmake/`, architecture decisions
in `docs/decisions/`, and approved vendored sources in `third_party/`. Treat
`build/`, `build-*`, and `cmake-build-*` as generated output; do not commit them.

## Build, Test, and Development Commands

Configure a release build:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

Compile all targets with `cmake --build build`. Run the complete test suite with
`ctest --test-dir build --output-on-failure`. Exercise the current CLI using
`./build/ltfa --help` or `./build/ltfa --version`. Compiler warnings are treated
as errors (`-Wall -Wextra -Wpedantic -Werror` on GCC/Clang), so a clean build is
part of validation.

## Coding Style & Naming Conventions

Format C++ with `clang-format -i <files>` using the repository `.clang-format`:
LLVM style, four-space indentation, 100-column lines, and case-sensitive include
sorting. Use `snake_case` for functions and local variables, `PascalCase` for
types, and `kPascalCase` for constants. Place code in the `ltfa` namespace and
prefer target-scoped CMake settings. Keep headers self-contained and expose only
stable core APIs through `include/ltfa/`.

## Testing Guidelines

Tests currently use dependency-free executables registered with CTest. Name test
sources descriptively (for example, `version_test.cpp`) and CTest cases with the
`ltfa.<area>[.<behavior>]` pattern. Add each test target to
`tests/CMakeLists.txt`. There is no coverage threshold yet; every behavior change
should include focused success and failure cases. Run the full CTest command
before submitting.

## Commit & Pull Request Guidelines

The repository has no commit history yet, so no established convention can be
inferred. Use short, imperative subjects such as `Add topology parser skeleton`,
and keep each commit to one reviewable concern. Pull requests should explain the
motivation and scope, link the relevant issue or design decision, list build and
test evidence, and include CLI output when user-visible behavior changes.

## Architecture & Security Boundaries

Preserve the one-shot, offline core boundary documented in
`docs/decisions/0001-bootstrap-boundary.md`. Do not add network probes, command
execution, databases, plugin loading, or vendored dependencies without an
approved design update and recorded license, version, source, and checksum.
