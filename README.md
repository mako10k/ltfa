# LTFA

LTFA is a lightweight multilayer topology and fault analyzer. It compiles a
strict local topology model and runs bounded offline path, impact, SPOF, and
declared-failure analysis.

LTFA v0.2.1 is a native C++17 one-shot CLI with no production runtime
dependencies beyond the C++ runtime. It does not access the network.

## Capabilities

- bounded LTFA DSL v1 parsing and model-root-safe local includes
- typed validation and deterministic normalized-model compilation
- human, `ltfa.model/v1` JSON, and DOT model output
- filtered reachability and deterministic representative paths
- modeled object, relation, and failure-domain removal impact
- requirement-group and failure-domain availability semantics
- immediate and fixed-delay declared-failure propagation
- articulation, bridge, and completeness-qualified SPOF analysis
- deterministic human and `ltfa.assurance/v1` assurance output

## Safety boundary

LTFA reads files below an explicit local model root. Included paths must be
relative and remain below that root after canonicalization. Defaults limit
include nesting to 16, each source to 16 MiB, and the complete load to 64 MiB.

LTFA does not:

- run PING, heartbeat, or other probes
- connect to production systems or the network
- execute commands supplied by topology input
- load binary monitoring plugins
- require a daemon, graph database, or runtime language interpreter
- infer a live failure cause from observations

Future observation producers remain separate processes connected through a
versioned data contract. The complete offline boundary is recorded in the
[bootstrap decision](docs/decisions/0001-bootstrap-boundary.md).

## Build and install

CMake 3.20 or later and a C++17 compiler are required.

```sh
cmake --preset release
cmake --build --preset release
./build/release/ltfa --version
```

Install into an explicit prefix:

```sh
cmake --install build/release --prefix /path/to/ltfa
/path/to/ltfa/bin/ltfa --version
```

The executable is installed under `bin`. License, README, release notes, and
the development guide are installed under `share/doc/ltfa`.

## Use

Discover the complete implemented command contract:

```sh
ltfa --help
ltfa help model
ltfa help analyze
ltfa help analyze --format json
```

Compile and render one local model:

```sh
ltfa model \
    --model-root topology \
    --input main.ltfa \
    --format json
```

Analyze an explicit scope, source, and target:

```sh
ltfa analyze \
    --model-root topology \
    --input main.ltfa \
    --scope site:a \
    --source service:source \
    --target service:target \
    --failure failure:breaker \
    --format json
```

The optional `--failure` value must identify a declared failure mode; LTFA does
not infer one. Positive work-budget options bound path traversal, SPOF
candidate checks, events, and state transitions. Restricted model IDs require
the explicit `--include-restricted` flag.

An incomplete budget-bounded analysis and an unreachable target are structured
successful results. Invalid model or analysis selections use status 4.
Invoking `ltfa` without a command is a usage error with status 2.

## Public contracts

- CLI Contract: 2
- DSL: LTFA DSL v1
- model JSON: `ltfa.model/v1`
- assurance JSON: `ltfa.assurance/v1`
- command help JSON: `ltfa.command-help/v1`
- CLI error JSON: `ltfa.cli-error/v1`
- process statuses: 0 result, 2 usage, 3 input, 4 validation, 5 output

The detailed assurance interface is recorded in the
[v0.2 CLI decision](docs/decisions/0003-v0.2-assurance-cli.md).

## Release status

`v0.2.1` is the current release. See the
[current release notes](RELEASE_NOTES.md) and
[GitHub release](https://github.com/mako10k/ltfa/releases/tag/v0.2.1) for the
included and excluded behavior. The accepted but unpublished v0.1.0 scope
remains in the [historical release notes](docs/releases/v0.1.0.md).

## Development

Repository setup, verification matrices, source layout, design sources,
planning, and release-maintainer guidance are in
[Developing LTFA](docs/development.md).

LTFA is licensed under the [MIT License](LICENSE).
