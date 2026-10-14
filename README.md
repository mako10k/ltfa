# LTFA

LTFA is a lightweight multilayer topology and fault analyzer. It compiles a
strict local topology model and runs bounded offline path, impact, SPOF, and
declared-failure analysis.

LTFA v0.3.0-alpha.1 is a native C++17 one-shot CLI with no production runtime
dependencies beyond the C++ runtime. It does not access the network.

## Capabilities

- bounded LTFA DSL v1 parsing and model-root-safe local includes
- typed validation and deterministic normalized-model compilation
- human, `ltfa.model/v1` JSON, lossless raw DOT, and semantic DOT views
- filtered reachability and deterministic representative paths
- modeled object, relation, and failure-domain removal impact
- requirement-group and failure-domain availability semantics
- first-class point-to-point links, LAG/bond aggregates, and STP/ring control domains
- bounded offline [Turbo Ring forwarding and post-failure reachability](docs/turbo-ring-forwarding.md)
- K-of-N aggregate availability with `mode=at-least minimum=N`
- immediate and fixed-delay declared-failure propagation
- articulation, bridge, and completeness-qualified SPOF analysis
- deterministic human and `ltfa.assurance/v1` assurance output
- strict bounded `ltfa.observation/v1` JSON Lines decoding through the core API
- explicit observation freshness and conflict indexing through the core API
- bounded causal-cone single-failure hypothesis ranking through the core API
- bounded object, relation, and common-domain service-cut analysis through the core API and CLI
- observation diagnosis and next-useful-observation recommendations through the core API and CLI
- descriptor-backed text and JSON help with deterministic error recovery actions
- descriptor-backed bash completion for commands, options, enums, and local path modes

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
- assert a confirmed live failure cause from observations

Observation producers remain separate processes connected through a versioned
data contract. The decoder, evidence index, and diagnosis engine do not run
them. Diagnosis returns qualified, evidence-ranked hypotheses rather than a
confirmed live cause. The `diagnose` CLI connects these local APIs without connecting to an
observation producer. The complete offline boundary is recorded in the
[bootstrap decision](docs/decisions/0001-bootstrap-boundary.md), and the
[observation contract](docs/decisions/0007-observation-json-lines-contract.md)
records the implemented decoder boundary.

Service-cut and next-observation analysis remain available as source-tree core APIs and are wired
to the `cuts` and `diagnose` commands. Cut analysis is limited to configured size and work budgets.
Observation recommendations select only registered probes that lack current evidence; they never
execute a probe or contact an observation producer.

## Build and install

CMake 3.20 or later, a C99 compiler, and a C++17 compiler are required. The C
compiler builds the pinned yyjson reader used by the core and CLI observation decoder.

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

The executable is installed under `bin`. License, README, release notes, the
[LTFA DSL v1 reference](docs/language/ltfa-dsl-v1.md),
[observation JSON Lines manual](docs/observations/ltfa-observation-v1.md), development guide, and
normative samples are installed under `share/doc/ltfa`. Bash completion is installed under
`share/bash-completion/completions/ltfa`; a configured `bash-completion` installation loads it
automatically.

## Use

Discover the complete implemented command contract:

```sh
ltfa --help
ltfa help model
ltfa help analyze
ltfa help analyze --format json
ltfa help cuts
ltfa help diagnose
ltfa help diagnose --format json
ltfa help guide
```

Learn the LTFA DSL v1 language, observation JSON Lines, and offline workflows through the bundled
Guide:

```sh
ltfa guide
ltfa guide syntax
ltfa guide observations
ltfa guide observations fields --level detail
ltfa guide statements relations --level detail
ltfa guide samples minimal
ltfa guide samples diagnosis
ltfa guide errors dsl --format json
```

`help` describes command contracts. `guide` provides topic/subtopic language guidance with
`index`, `quick`, and `detail` levels and stable `ltfa.language-guide/v1` JSON. Both are generated
from immutable descriptors.

Human errors provide bounded next-action navigation without applying a fix or
automatically retrying an operation:

```text
LTCLI-001 error: unknown option --formt
  token: --formt
  suggestion: --format
  next: Replace --formt with --format, then rerun ltfa model.
  help: ltfa help model
```

For source-tree development, enable bash completion with:

```bash
export LTFA_COMPLETION_BIN="$PWD/build/release/ltfa"
source completions/ltfa.bash
```

Completion candidates come from the same command descriptors as help and parsing, and Guide topics,
observation subtopics, and sample IDs come from the same catalog as Guide output. LTFA does not load
a model or enumerate topology IDs during completion.

Compile and render one local model:

```sh
ltfa model \
    --model-root topology \
    --input main.ltfa \
    --format json
```

Emit compatible lossless DOT, or select a smaller semantic relation lens:

```sh
ltfa model --model-root topology --input main.ltfa --format dot
ltfa model --model-root topology --input main.ltfa \
    --format dot --dot-view topology
ltfa model --model-root topology --input main.ltfa \
    --format dot --dot-view power
ltfa model --model-root topology --input main.ltfa \
    --format dot --dot-view logical
ltfa model --model-root topology --input main.ltfa \
    --format dot --dot-view protection
```

The default `raw` DOT view keeps every selected relation as an edge and uses
schema `ltfa.model.dot/v1`. Semantic `topology`, `power`, `dependency`, and
`overlay` views use `ltfa.diagram.dot/v1`: eligible `contained_in` relations
become nested clusters, owned endpoints become ports, and only the selected
relation lens remains as normal edges. The `logical` view instead folds
aggregate membership, while `protection` folds STP or ring-domain membership
and keeps registered non-forwarding segments as dashed context. `--dot-view`
is valid only with `--format dot`. LTFA emits DOT text and does not invoke
Graphviz.

The normative [resilience sample](examples/resilience/main.ltfa)
demonstrates first-class physical links, a two-member LACP aggregate with a
one-member threshold, and an RSTP baseline. `normal-state` is registered design
state, not a live observation. The complete modeling decision and fail-closed
rules are in [ADR 0005](docs/decisions/0005-link-aggregation-and-protection-domains.md).

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

Find bounded service cuts for one scoped source and target:

```sh
ltfa cuts --model-root topology --input main.ltfa --scope site:a \
    --source service:source --target service:target --format json
```

Diagnose a bounded local observation file and include next-useful-observation recommendations:

```sh
ltfa diagnose --model-root topology --input main.ltfa \
    --observations observations.jsonl --analysis-at 2030-01-01T00:10:30Z --format json
```

The observation path must remain below the model root. `--analysis-at` is explicit: LTFA does not
read a clock. Recommendations identify registered probes but never execute them. A complete
synthetic example is available under `examples/diagnosis`:

```sh
ltfa model --model-root examples/diagnosis --input main.ltfa
ltfa diagnose --model-root examples/diagnosis --input main.ltfa \
    --observations observations.jsonl --analysis-at 2030-01-01T00:10:30Z --format json
```

The eight required fields are `schema`, `probe_id`, `target`, `vantage`, `kind`, `outcome`,
`observed_at`, and `valid_for_ms`. See `ltfa guide observations`, the
[observation manual](docs/observations/ltfa-observation-v1.md), and `ltfa help diagnose` for the
synchronized field, enum, error-recovery, and offline contracts.

## Public contracts

- CLI Contract: 5
- DSL: LTFA DSL v1
- Observation input: `ltfa.observation/v1` JSON Lines core API
- Observation analysis: C++17 evidence-index and bounded-diagnosis core APIs
- model JSON: `ltfa.model/v1`
- raw model DOT: `ltfa.model.dot/v1`
- semantic diagram DOT: `ltfa.diagram.dot/v1`
- assurance JSON: `ltfa.assurance/v1`
- service cuts JSON: `ltfa.service-cuts/v1`
- diagnosis CLI JSON: `ltfa.diagnosis-cli/v1`
- command help JSON: `ltfa.command-help/v1`
- language guide JSON: `ltfa.language-guide/v1`
- CLI error JSON: `ltfa.cli-error/v1`
- process statuses: 0 result, 2 usage, 3 input, 4 validation, 5 output

The detailed assurance interface is recorded in the
[v0.2 CLI decision](docs/decisions/0003-v0.2-assurance-cli.md).

## Release status

`v0.3.0-alpha.1` is the current prerelease checkpoint. See the
[current prerelease notes](RELEASE_NOTES.md) and
[GitHub tag](https://github.com/mako10k/ltfa/releases/tag/v0.3.0-alpha.1) for
the included and excluded behavior. `v0.2.1` remains the current stable
release, with notes preserved in
[the release archive](docs/releases/v0.2.1.md).

## Development

Repository setup, verification matrices, source layout, design sources,
planning, and release-maintainer guidance are in
[Developing LTFA](docs/development.md).

LTFA is licensed under the [MIT License](LICENSE).
