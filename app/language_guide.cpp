#include "language_guide.hpp"

#include "cli_contract.hpp"

#include "ltfa/version.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <ostream>
#include <string>
#include <vector>

namespace ltfa::app {
namespace {

constexpr char kItemSeparator = '\x1e';

constexpr ObservationGuideDescriptor kObservationGuide{
    "ltfa.observation/v1",
    "json-lines",
    "schema\x1e"
    "probe_id\x1e"
    "target\x1e"
    "vantage\x1e"
    "kind\x1e"
    "outcome\x1e"
    "observed_at\x1e"
    "valid_for_ms",
    "duration_ms\x1e"
    "value\x1e"
    "unit\x1e"
    "state\x1e"
    "producer_sequence\x1e"
    "producer_id\x1e"
    "evidence_ref\x1e"
    "detail",
    "icmp\x1e"
    "tcp-connect\x1e"
    "custom-heartbeat\x1e"
    "metric\x1e"
    "discrete-state",
    "pass\x1e"
    "fail\x1e"
    "timeout\x1e"
    "error\x1e"
    "unknown",
    "ltfa guide observations",
    "ltfa guide observations errors",
    "share/doc/ltfa/docs/observations/ltfa-observation-v1.md",
    "share/doc/ltfa/examples/diagnosis",
};

constexpr std::array<GuidePageDescriptor, 25> kGuidePages{{
    {"syntax", "", "DSL syntax", "LTFA DSL v1 document shape, lexical rules, values, and comments.",
     "Every source begins with exactly 'ltfa 1'.\x1e"
     "Statements are line-oriented; blank lines and text after # are ignored.\x1e"
     "Use quoted strings for labels and quoted relative paths for include.",
     "ltfa 1\x1e"
     "<statement> [attribute=value ...]\x1e"
     "# comment",
     "UTF-8 is required and a UTF-8 BOM is rejected.\x1e"
     "Quoted strings end on the same line and do not define escape sequences.\x1e"
     "Bare values can be identifiers, integers, reals, booleans, or durations.",
     "ltfa 1\nscope site:example kind=site completeness=verified "
     "evidence-quality=confirmed",
     "ltfa guide statements\x1eltfa guide identifiers\x1eltfa guide attributes"},
    {"statements", "", "Statements",
     "The nine LTFA DSL v1 statement forms and their required positional fields.",
     "Declare scopes and objects before referring to them.\x1e"
     "connect accepts -- or ->; relate accepts -> only.\x1e"
     "Attributes follow required positional fields as key=value pairs.",
     "scope <id> [attributes]\x1e"
     "entity <id> [attributes]\x1e"
     "endpoint <id> owner=<entity-id> [attributes]\x1e"
     "connect <object-id> (--|->) <object-id> [attributes]\x1e"
     "relate <object-id> -> <object-id> [attributes]\x1e"
     "require <consumer-id> by <provider-id> [attributes]\x1e"
     "failure <id> target=<object-id> [attributes]\x1e"
     "probe <id> target=<object-id> vantage=<entity-id> [attributes]\x1e"
     "include \"<relative-path>\"",
     "scope, entity, endpoint, relation, requirement, failure, and probe references are "
     "validated after includes are expanded.\x1e"
     "A relation requires kind=. A requirement requires group= and mode=.\x1e"
     "A failure requires mode=. A probe requires kind= and valid-for=.",
     "entity device:router kind=device plane=communication layer=physical "
     "scope=site:example\x1e"
     "connect device:router:lan -- device:switch:lan kind=network",
     "ltfa guide statements declarations\x1e"
     "ltfa guide statements relations\x1e"
     "ltfa guide statements assurance"},
    {"statements", "declarations", "Scope and object declarations",
     "Declare a scope, entities in that scope, and endpoints owned by entities.",
     "scope metadata qualifies later assurance results.\x1e"
     "entity carries kind, plane, layer, and scope metadata.\x1e"
     "endpoint must name its owner with owner=<entity-id>.",
     "scope <id> kind=site|cloud|shared|other "
     "completeness=verified|partial|unknown evidence-quality=confirmed|inferred|unknown\x1e"
     "entity <id> kind=<entity-kind> plane=<plane> layer=<layer> scope=<scope-id>\x1e"
     "endpoint <id> owner=<entity-id> kind=<endpoint-kind>",
     "The compiler supplies documented defaults for omitted enum attributes, but explicit scope "
     "completeness and evidence quality make assurance qualification clear.\x1e"
     "Entity and endpoint IDs share one object namespace.",
     "scope site:a kind=site completeness=verified evidence-quality=confirmed\n"
     "entity device:r1 kind=device plane=communication layer=physical scope=site:a\n"
     "endpoint device:r1:lan owner=device:r1 kind=network",
     "ltfa guide attributes\x1eltfa guide identifiers"},
    {"statements", "relations", "Connection and relation declarations",
     "Connect physical or logical objects and declare directed semantic relations.",
     "Use -- for a bidirectional connection and -> for a directed connection.\x1e"
     "Use relate for directed semantic relations such as depends_on or member_of.\x1e"
     "Every connection and relation requires kind=.",
     "connect <from> -- <to> kind=<relation-kind>\x1e"
     "connect <from> -> <to> kind=<relation-kind>\x1e"
     "relate <from> -> <to> kind=<relation-kind>",
     "Built-in relation kinds are network, power, serial, contact, analog-current, member_of, "
     "contained_in, carried_by, uses_underlay, terminates_on, routes_via, depends_on, and "
     "powered_by.\x1e"
     "role and normal-state are validated for supported aggregate and control-domain relations.",
     "relate service:app -> process:worker kind=depends_on",
     "ltfa guide statements\x1eltfa guide attributes"},
    {"statements", "assurance", "Requirement, failure, and probe declarations",
     "Declare availability requirements, modeled failure modes, and registered observation probes.",
     "require groups providers for one consumer.\x1e"
     "failure identifies one target and modeled mode.\x1e"
     "probe identifies a target, vantage point, kind, and evidence validity duration.",
     "require <consumer> by <provider> group=<id> mode=all|any|at-least "
     "[minimum=<positive-integer>] [hold-up=<duration>]\x1e"
     "failure <id> target=<object-id> mode=<failure-mode> [after=<duration>]\x1e"
     "probe <id> target=<object-id> vantage=<entity-id> kind=<probe-kind> valid-for=<duration>",
     "minimum is required only for mode=at-least.\x1e"
     "Failure modes beginning x- are retained as extensions.\x1e"
     "Probe declarations register evidence sources; LTFA never runs the probes.",
     "require service:app by process:worker group=runtime mode=all hold-up=30s\n"
     "failure failure:worker target=process:worker mode=device-failure\n"
     "probe probe:worker target=process:worker vantage=process:collector kind=icmp valid-for=60s",
     "ltfa guide workflows analyze\x1eltfa guide workflows diagnose"},
    {"identifiers", "", "Identifiers",
     "Stable IDs and attribute keys use bounded lowercase ASCII grammars.",
     "IDs must match [a-z0-9][a-z0-9._:-]*.\x1e"
     "Attribute keys must match [a-z][a-z0-9-]*.\x1e"
     "Use descriptive namespace prefixes such as site:, device:, service:, failure:, and probe:.",
     "identifier := [a-z0-9][a-z0-9._:-]*\x1e"
     "attribute-key := [a-z][a-z0-9-]*\x1e"
     "extension-key := x-[a-z0-9-]+",
     "Quoted text does not relax identifier grammar when a field requires an ID.\x1e"
     "References must resolve to a declaration of the required object kind.\x1e"
     "Restricted IDs require --include-restricted before user-visible output can expose them.",
     "site:plant-a\x1e"
     "device:router_1\x1e"
     "failure:wan-link",
     "ltfa guide errors validation\x1eltfa guide attributes"},
    {"attributes", "", "Attributes",
     "Statement-specific key=value metadata, typed values, and explicit extension keys.",
     "Known keys are restricted by statement kind.\x1e"
     "Unknown keys are accepted only with the x- extension prefix.\x1e"
     "failure-domain can repeat on entity and endpoint; other attributes cannot repeat.",
     "key=<identifier|string|integer|real|boolean|duration>\x1e"
     "duration := <digits>(ms|s|m|h|d)\x1e"
     "string := \"UTF-8 text on one line\"",
     "scope keys: kind, label, completeness, evidence-quality.\x1e"
     "entity keys: kind, plane, layer, scope, label, network-space, failure-domain, sensitivity, "
     "source-ref, address, medium, protocol, mode, role.\x1e"
     "endpoint keys: kind, scope, label, network-space, failure-domain, sensitivity, source-ref, "
     "address, medium, protocol, role, normal-state, current-range.\x1e"
     "relation keys: kind, label, scope, sensitivity, source-ref, role, normal-state.\x1e"
     "require keys: group, mode, minimum, hold-up. failure keys: mode, after. probe keys: kind, "
     "valid-for.",
     "entity device:r1 kind=device label=\"Router 1\" x-inventory-ref=asset-42",
     "ltfa guide statements\x1eltfa guide identifiers\x1eltfa guide errors validation"},
    {"includes", "", "Local includes",
     "Compose a model from quoted relative files while remaining below the explicit model root.",
     "Use include \"relative/path.ltfa\".\x1e"
     "Each included source has its own exact ltfa 1 header.\x1e"
     "Paths are resolved canonically and must remain below --model-root.",
     "include \"facility.ltfa\"\x1einclude \"layers/network.ltfa\"",
     "Absolute paths, URI schemes, root escapes, non-regular files, cycles, and duplicate "
     "canonical "
     "sources are rejected.\x1e"
     "Default bounds are depth 16, 16 MiB per source, 64 MiB total, and 8 KiB per line.\x1e"
     "The entry file is also selected relative to --model-root.",
     "ltfa 1\nscope site:a kind=site\ninclude \"objects.ltfa\"",
     "ltfa guide errors include\x1eltfa guide workflows model"},
    {"samples", "", "Normative samples",
     "Logical sample IDs for small validated model trees shipped with the source and installation.",
     "minimal demonstrates the smallest useful dependency model.\x1e"
     "resilience demonstrates links, a LAG, a control domain, K-of-N requirements, and failures.",
     "ltfa guide samples minimal\x1eltfa guide samples resilience",
     "Repository locations are examples/<sample-id>/main.ltfa.\x1e"
     "Installed locations are share/doc/ltfa/examples/<sample-id>/main.ltfa.",
     "minimal\x1eresilience", "ltfa guide workflows model\x1eltfa guide syntax"},
    {"samples", "minimal", "Sample: minimal", "A small two-object LTFA DSL v1 dependency model.",
     "Copy the complete sample tree, then select its directory as --model-root.\x1e"
     "Validate before using its IDs in an analysis command.",
     "ltfa 1\x1e"
     "scope site:example kind=site completeness=verified evidence-quality=confirmed\x1e"
     "entity service:app kind=service plane=service layer=application scope=site:example\x1e"
     "entity process:worker kind=process plane=service layer=application scope=site:example\x1e"
     "relate service:app -> process:worker kind=depends_on",
     "Repository: examples/minimal/main.ltfa.\x1e"
     "Installed: share/doc/ltfa/examples/minimal/main.ltfa.",
     "ltfa model --model-root examples/minimal --input main.ltfa",
     "ltfa guide workflows model\x1eltfa guide statements"},
    {"samples", "resilience", "Sample: resilience",
     "A bounded resilience model with independent links and aggregate/control metadata.",
     "Use the sample to inspect raw or semantic DOT views.\x1e"
     "The two links have distinct failure domains and feed a one-of-two aggregate requirement.",
     "entity link:1 kind=link ... failure-domain=cable-route:east\x1e"
     "entity lag:uplink kind=aggregate ... protocol=lacp mode=active-active\x1e"
     "require lag:uplink by link:1 group=members mode=at-least minimum=1",
     "Repository: examples/resilience/main.ltfa.\x1e"
     "Installed: share/doc/ltfa/examples/resilience/main.ltfa.",
     "ltfa model --model-root examples/resilience --input main.ltfa --format dot "
     "--dot-view protection",
     "ltfa guide statements relations\x1eltfa guide workflows analyze"},
    {"samples", "diagnosis", "Sample: diagnosis",
     "A complete synthetic model, registered probe, and ltfa.observation/v1 JSON Lines input.",
     "Copy the complete diagnosis sample tree.\x1e"
     "Validate the model before running diagnosis.\x1e"
     "Pass an explicit --analysis-at later than the sample observation time.",
     "ltfa model --model-root examples/diagnosis --input main.ltfa\x1e"
     "ltfa diagnose --model-root examples/diagnosis --input main.ltfa "
     "--observations observations.jsonl --analysis-at 2030-01-01T00:10:30Z",
     "Repository: examples/diagnosis.\x1e"
     "Installed: share/doc/ltfa/examples/diagnosis.\x1e"
     "The sample is synthetic and does not confirm a live failure cause.",
     "ltfa guide observations fields\x1eltfa help diagnose",
     "ltfa guide observations\x1eltfa guide workflows diagnose"},
    {"errors", "", "Diagnostics",
     "Stable diagnostic families for syntax, include loading, and normalized-model validation.",
     "Read the first stable code and source location.\x1e"
     "Correct the local input, then rerun the same read-only command.\x1e"
     "Use the guide route printed beside the error for focused rules.",
     "E_DSL_* := lexical or statement grammar\x1e"
     "E_INCLUDE_* := local include loading and containment\x1e"
     "E_ID_*|E_REFERENCE_*|other E_* := model validation",
     "Human errors keep stdout empty and report on stderr.\x1e"
     "With --format json, errors use ltfa.cli-error/v1 and ordered navigation actions.",
     "main.ltfa:2:1: error E_DSL_UNKNOWN_STATEMENT: unknown DSL statement 'object'",
     "ltfa guide errors dsl\x1eltfa guide errors include\x1eltfa guide errors validation"},
    {"errors", "dsl", "DSL diagnostics",
     "Lexical, header, statement, attribute, and value errors in one local source.",
     "Verify the exact ltfa 1 header.\x1e"
     "Verify one supported statement keyword per non-empty line.\x1e"
     "Verify required positional fields before key=value attributes.",
     "E_DSL_INVALID_HEADER\x1e"
     "E_DSL_UNKNOWN_STATEMENT\x1e"
     "E_DSL_UNEXPECTED_TOKEN\x1e"
     "E_DSL_MISSING_REQUIRED_FIELD\x1e"
     "E_DSL_INVALID_ATTRIBUTE\x1e"
     "E_DSL_INVALID_VALUE",
     "The diagnostic source points to the earliest rejected token.\x1e"
     "Quoted strings cannot continue across lines.",
     "ltfa guide syntax --level detail\x1eltfa guide statements --level detail",
     "ltfa guide syntax\x1eltfa guide statements\x1eltfa guide attributes"},
    {"errors", "include", "Include diagnostics",
     "Containment, I/O, cycle, duplication, depth, and total-size failures while loading a tree.",
     "Keep every path relative and below --model-root.\x1e"
     "Check that each selected path is a regular file.\x1e"
     "Remove cycles and duplicate canonical references.",
     "E_INCLUDE_PATH\x1e"
     "E_INCLUDE_IO\x1e"
     "E_INCLUDE_ESCAPE\x1e"
     "E_INCLUDE_CYCLE\x1e"
     "E_INCLUDE_DUPLICATE\x1e"
     "E_INCLUDE_DEPTH",
     "No URI or network include scheme is supported.\x1e"
     "A symlink that resolves outside the model root is rejected.",
     "ltfa guide includes --level detail", "ltfa guide includes\x1eltfa guide workflows model"},
    {"errors", "validation", "Model-validation diagnostics",
     "Duplicate IDs, unresolved references, incompatible layers, and invalid semantic attributes.",
     "Declare referenced scopes and objects.\x1e"
     "Use the enum values documented by the relevant statement and attribute page.\x1e"
     "Keep owner, scope, relation, aggregate, and requirement semantics consistent.",
     "E_ID_*\x1e"
     "E_REFERENCE_*\x1e"
     "E_LAYER_*\x1e"
     "E_DEPENDENCY_*\x1e"
     "E_REQUIREMENT_*",
     "Parsing success does not imply semantic validation success.\x1e"
     "Run ltfa model first to validate and summarize the normalized model.",
     "ltfa model --model-root topology --input main.ltfa",
     "ltfa guide attributes\x1eltfa guide workflows model"},
    {"observations", "", "Observation JSON Lines",
     "Author bounded ltfa.observation/v1 JSON Lines for offline diagnosis.",
     "Use one UTF-8 JSON object per physical line.\x1e"
     "Keep observation production outside LTFA.\x1e"
     "Pass --analysis-at explicitly when diagnosing the local file.",
     "ltfa guide observations format\x1e"
     "ltfa guide observations fields\x1e"
     "ltfa guide observations semantics\x1e"
     "ltfa guide observations errors",
     "Observation JSON Lines is a separate data contract, not LTFA DSL v1.\x1e"
     "The decoder returns no partial observation batch on any rejected record.",
     "ltfa guide samples diagnosis\x1eltfa help diagnose",
     "ltfa guide workflows diagnose\x1eltfa guide samples diagnosis"},
    {"observations", "format", "Observation framing and bounds",
     "Encode one strict ltfa.observation/v1 object on each bounded UTF-8 line.",
     "Use LF or CRLF line endings.\x1e"
     "Do not add blank interior lines, comments, JSON5, trailing commas, NaN, or infinity.\x1e"
     "Treat one final line delimiter as optional.",
     "{\"schema\":\"ltfa.observation/v1\",...}\x1e"
     "{\"schema\":\"ltfa.observation/v1\",...}",
     "Defaults bound the complete input to 64 MiB, one record to 64 KiB, and the batch to "
     "100000 records.\x1e"
     "Any JSON, UTF-8, schema, semantic, or limit error rejects the complete batch.",
     "ltfa guide samples diagnosis",
     "ltfa guide observations fields\x1eltfa guide observations errors"},
    {"observations", "fields", "Observation fields",
     "Use all eight required fields and only the documented optional or x- extension fields.",
     "Required: schema, probe_id, target, vantage, kind, outcome, observed_at, valid_for_ms.\x1e"
     "Optional: duration_ms, value, unit, state, producer_sequence, producer_id, evidence_ref, "
     "detail.\x1e"
     "Extensions use bounded lowercase x- keys and scalar values only.",
     "schema=ltfa.observation/v1\x1e"
     "kind=icmp|tcp-connect|custom-heartbeat|metric|discrete-state\x1e"
     "outcome=pass|fail|timeout|error|unknown",
     "value requires unit and is valid only for kind=metric.\x1e"
     "state is valid only for kind=discrete-state.\x1e"
     "Secret-bearing extension names and nested arrays or objects are rejected.",
     "ltfa guide samples diagnosis",
     "ltfa guide observations format\x1eltfa guide observations semantics"},
    {"observations", "semantics", "Observation semantics",
     "Interpret observations as time-bounded evidence rather than confirmed live causes.",
     "Use an RFC3339 observed_at with a known offset.\x1e"
     "Compare freshness with explicit --analysis-at and valid_for_ms.\x1e"
     "Keep probe_id, target, vantage, and kind consistent with the registered model probe.",
     "observed_at=<RFC3339-timestamp>\x1e"
     "valid_for_ms=<unsigned-integer>\x1e"
     "--analysis-at <RFC3339-timestamp>",
     "A timeout remains a typed outcome; it is not proof that a device is down.\x1e"
     "Conflicting current observations remain qualified evidence.\x1e"
     "A next-observation recommendation names a registered probe but never executes it.",
     "ltfa diagnose --model-root examples/diagnosis --input main.ltfa "
     "--observations observations.jsonl --analysis-at 2030-01-01T00:10:30Z",
     "ltfa guide workflows diagnose\x1eltfa guide observations errors"},
    {"observations", "errors", "Observation diagnostics",
     "Recover from local observation loading, timestamp, decoding, and evidence-contract errors.",
     "LTOBS-001: resolve a contained local observation file.\x1e"
     "LTOBS-002: correct the explicit RFC3339 analysis time.\x1e"
     "LTOBS-003: correct JSON Lines framing, schema, fields, or limits.\x1e"
     "LTOBS-004: align observations with registered probes and explicit reference time.",
     "correct local input -> rerun ltfa diagnose\x1e"
     "inspect guide -> ltfa guide observations errors\x1e"
     "inspect help -> ltfa help diagnose",
     "The diagnostic and Guide never edit input or retry diagnosis.\x1e"
     "Structured failures retain ltfa.cli-error/v1 and ordered navigation actions.",
     "ltfa guide observations fields\x1eltfa help diagnose",
     "ltfa guide observations\x1eltfa guide samples diagnosis"},
    {"workflows", "", "Offline workflows",
     "Use validation before bounded analysis and keep every operation local and one-shot.",
     "Start with ltfa model.\x1e"
     "Use declared IDs only after validation succeeds.\x1e"
     "Select JSON for automation and human or DOT output for inspection.",
     "model -> analyze|cuts\x1emodel + observations -> diagnose",
     "LTFA does not discover systems, run probes, execute commands, or access the network.\x1e"
     "Restricted IDs require explicit --include-restricted output authority.",
     "ltfa model --model-root topology --input main.ltfa",
     "ltfa guide workflows model\x1eltfa guide workflows analyze\x1e"
     "ltfa guide workflows diagnose"},
    {"workflows", "model", "Validate and render a model",
     "Load one explicit local model tree, validate it, and render deterministic output.",
     "Validate with human output first.\x1e"
     "Use JSON for normalized-model automation.\x1e"
     "Use DOT plus --dot-view for raw or semantic graph inspection.",
     "ltfa model --model-root <directory> --input <relative-file>\x1e"
     "ltfa model ... --format json\x1eltfa model ... --format dot --dot-view topology",
     "The command reads only the selected local model tree.\x1e"
     "LTFA emits DOT text but does not invoke Graphviz.",
     "ltfa model --model-root examples/minimal --input main.ltfa",
     "ltfa guide samples minimal\x1eltfa help model"},
    {"workflows", "analyze", "Run bounded assurance analysis",
     "Validate the model, then select one scope, source, target, and optional declared failure.",
     "Use IDs emitted by a successful model operation.\x1e"
     "Keep source and target distinct and inside the selected scope.\x1e"
     "Increase work budgets only with an explicit bounded reason.",
     "ltfa analyze --model-root <directory> --input <file> --scope <scope-id> "
     "--source <object-id> --target <object-id> [--failure <failure-id>]",
     "The result separates complete and bounded-incomplete analysis.\x1e"
     "A declared failure is simulated; it is not asserted as a live cause.",
     "ltfa help analyze", "ltfa guide statements assurance\x1eltfa help analyze\x1eltfa help cuts"},
    {"workflows", "diagnose", "Diagnose local observations",
     "Combine one validated local model with explicit observation JSON Lines and reference time.",
     "Keep observation production outside LTFA.\x1e"
     "Pass --analysis-at explicitly; LTFA does not read a clock.\x1e"
     "Treat recommendations as next useful observations, not executed probes.",
     "ltfa diagnose --model-root <directory> --input <file> --observations <file> "
     "--analysis-at <RFC3339-timestamp>",
     "The observation file uses ltfa.observation/v1 JSON Lines, not LTFA DSL v1.\x1e"
     "The diagnosis is evidence-qualified and does not confirm a live root cause.",
     "ltfa help diagnose",
     "ltfa guide observations\x1eltfa guide samples diagnosis\x1eltfa help diagnose"},
}};

template <typename Callback> void for_each_item(std::string_view items, Callback callback) {
    std::size_t begin = 0;
    while (begin < items.size()) {
        const auto end = items.find(kItemSeparator, begin);
        const auto length = end == std::string_view::npos ? items.size() - begin : end - begin;
        if (length != 0) {
            callback(items.substr(begin, length));
        }
        if (end == std::string_view::npos) {
            break;
        }
        begin = end + 1;
    }
}

void print_json_string(std::ostream &output, std::string_view value) {
    constexpr char kHexDigits[] = "0123456789abcdef";
    output << '"';
    for (const unsigned char byte : value) {
        switch (byte) {
        case '"':
            output << "\\\"";
            break;
        case '\\':
            output << "\\\\";
            break;
        case '\b':
            output << "\\b";
            break;
        case '\f':
            output << "\\f";
            break;
        case '\n':
            output << "\\n";
            break;
        case '\r':
            output << "\\r";
            break;
        case '\t':
            output << "\\t";
            break;
        default:
            if (byte < 0x20U) {
                output << "\\u00" << kHexDigits[(byte >> 4U) & 0x0FU] << kHexDigits[byte & 0x0FU];
            } else {
                output << static_cast<char>(byte);
            }
            break;
        }
    }
    output << '"';
}

void print_json_items(std::ostream &output, std::string_view items) {
    output << '[';
    std::size_t index = 0;
    for_each_item(items, [&output, &index](std::string_view item) {
        if (index++ != 0) {
            output << ',';
        }
        print_json_string(output, item);
    });
    output << ']';
}

void print_text_items(std::ostream &output, std::string_view title, std::string_view items) {
    if (items.empty()) {
        return;
    }
    output << '\n' << title << ":\n";
    for_each_item(items, [&output](std::string_view item) {
        output << "- ";
        for (const char value : item) {
            output << value;
            if (value == '\n') {
                output << "  ";
            }
        }
        output << '\n';
    });
}

std::vector<const GuidePageDescriptor *> topics() {
    std::vector<const GuidePageDescriptor *> result;
    for (const auto &page : kGuidePages) {
        if (page.subtopic.empty()) {
            result.push_back(&page);
        }
    }
    return result;
}

std::vector<const GuidePageDescriptor *> subtopics(std::string_view topic) {
    std::vector<const GuidePageDescriptor *> result;
    for (const auto &page : kGuidePages) {
        if (page.topic == topic && !page.subtopic.empty()) {
            result.push_back(&page);
        }
    }
    return result;
}

std::size_t edit_distance(std::string_view left, std::string_view right) {
    std::vector<std::size_t> previous(right.size() + 1);
    std::vector<std::size_t> current(right.size() + 1);
    for (std::size_t index = 0; index <= right.size(); ++index) {
        previous[index] = index;
    }
    for (std::size_t left_index = 1; left_index <= left.size(); ++left_index) {
        current[0] = left_index;
        for (std::size_t right_index = 1; right_index <= right.size(); ++right_index) {
            const auto substitution = previous[right_index - 1] +
                                      (left[left_index - 1] == right[right_index - 1] ? 0 : 1);
            current[right_index] =
                std::min({previous[right_index] + 1, current[right_index - 1] + 1, substitution});
        }
        previous.swap(current);
    }
    return previous.back();
}

template <typename Pages>
std::string nearest_page(std::string_view value, const Pages &pages, bool use_subtopic) {
    std::size_t best_distance = std::numeric_limits<std::size_t>::max();
    std::string_view best;
    bool tied = false;
    for (const auto *page : pages) {
        const auto candidate = use_subtopic ? page->subtopic : page->topic;
        const auto distance = edit_distance(value, candidate);
        if (distance < best_distance) {
            best_distance = distance;
            best = candidate;
            tied = false;
        } else if (distance == best_distance) {
            tied = true;
        }
    }
    const auto threshold = std::max<std::size_t>(1, value.size() / 3);
    return !tied && best_distance <= threshold ? std::string{best} : std::string{};
}

void print_topic_index_text(std::ostream &output, const GuidePageDescriptor *page) {
    const auto entries = page == nullptr ? topics() : subtopics(page->topic);
    output << '\n' << (page == nullptr ? "Topics" : "Subtopics") << ":\n";
    if (entries.empty()) {
        output << "  (none)\n";
        return;
    }
    for (const auto *entry : entries) {
        output << "  " << (page == nullptr ? entry->topic : entry->subtopic) << "  "
               << entry->summary << '\n';
    }
}

void print_topic_index_json(std::ostream &output, std::string_view key,
                            const std::vector<const GuidePageDescriptor *> &entries,
                            bool use_subtopic) {
    output << ",\"" << key << "\":[";
    for (std::size_t index = 0; index < entries.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        output << "{\"id\":";
        print_json_string(output, use_subtopic ? entries[index]->subtopic : entries[index]->topic);
        output << ",\"title\":";
        print_json_string(output, entries[index]->title);
        output << ",\"summary\":";
        print_json_string(output, entries[index]->summary);
        output << '}';
    }
    output << ']';
}

std::string_view guide_domain(const GuidePageDescriptor *page) {
    if (page == nullptr) {
        return "guide";
    }
    if (page->topic == "observations" ||
        (page->topic == "samples" && page->subtopic == "diagnosis")) {
        return "observation-data";
    }
    if (page->topic == "workflows") {
        return "workflow";
    }
    return "model-language";
}

void print_observation_contract_json(std::ostream &output, const GuidePageDescriptor *page) {
    output << ",\"observation_contract\":";
    if (page == nullptr || page->topic != "observations") {
        output << "null";
        return;
    }
    output << "{\"schema\":";
    print_json_string(output, kObservationGuide.schema);
    output << ",\"framing\":";
    print_json_string(output, kObservationGuide.framing);
    output << ",\"required_fields\":";
    print_json_items(output, kObservationGuide.required_fields);
    output << ",\"optional_fields\":";
    print_json_items(output, kObservationGuide.optional_fields);
    output << ",\"kinds\":";
    print_json_items(output, kObservationGuide.kinds);
    output << ",\"outcomes\":";
    print_json_items(output, kObservationGuide.outcomes);
    output << '}';
}

} // namespace

GuideCatalog guide_catalog() { return {kGuidePages.data(), kGuidePages.size()}; }

const ObservationGuideDescriptor &observation_guide_descriptor() { return kObservationGuide; }

const GuidePageDescriptor *find_guide_page(std::string_view topic, std::string_view subtopic) {
    const auto found = std::find_if(kGuidePages.begin(), kGuidePages.end(),
                                    [topic, subtopic](const GuidePageDescriptor &page) {
                                        return page.topic == topic && page.subtopic == subtopic;
                                    });
    return found == kGuidePages.end() ? nullptr : &*found;
}

std::string suggest_guide_topic(std::string_view topic) {
    return topic.size() > 128 ? std::string{} : nearest_page(topic, topics(), false);
}

std::string suggest_guide_subtopic(std::string_view topic, std::string_view subtopic) {
    return subtopic.size() > 128 ? std::string{} : nearest_page(subtopic, subtopics(topic), true);
}

std::string_view to_string(GuideLevel level) noexcept {
    switch (level) {
    case GuideLevel::index:
        return "index";
    case GuideLevel::quick:
        return "quick";
    case GuideLevel::detail:
        return "detail";
    }
    return "unknown";
}

GuideLevel default_guide_level(std::string_view topic, std::string_view subtopic) {
    if (topic.empty()) {
        return GuideLevel::index;
    }
    return subtopic.empty() ? GuideLevel::quick : GuideLevel::detail;
}

void print_language_guide_text(std::ostream &output, const GuidePageDescriptor *page,
                               GuideLevel level) {
    output << (page == nullptr ? "LTFA Guide" : page->title) << '\n'
           << "\n"
           << "Topic: " << (page == nullptr ? "overview" : page->topic) << '\n';
    if (page != nullptr && !page->subtopic.empty()) {
        output << "Subtopic: " << page->subtopic << '\n';
    }
    output << "Level: " << to_string(level) << "\n\n"
           << (page == nullptr ? "Bundled model-language, observation-data, and workflow guidance."
                               : page->summary)
           << '\n';

    if (level == GuideLevel::index) {
        print_topic_index_text(output, page);
    } else if (page != nullptr) {
        print_text_items(output, "Quick Reference", page->quick_reference);
        print_text_items(output, "Syntax", page->syntax);
        if (level == GuideLevel::detail) {
            print_text_items(output, "Notes", page->notes);
            print_text_items(output, "Examples", page->examples);
        }
        const auto entries = subtopics(page->topic);
        if (page->subtopic.empty() && !entries.empty()) {
            print_topic_index_text(output, page);
        }
        print_text_items(output, "Related", page->related);
    }

    output << "\nNext Requests:\n";
    if (page == nullptr) {
        output << "  ltfa guide <topic>\n"
               << "  ltfa guide <topic> <subtopic> --level detail\n";
    } else {
        output << "  ltfa guide " << page->topic;
        if (!page->subtopic.empty()) {
            output << ' ' << page->subtopic;
        }
        output << " --level detail\n"
               << "  ltfa help guide\n";
    }
}

void print_language_guide_json(std::ostream &output, const GuidePageDescriptor *page,
                               GuideLevel level) {
    output << "{\"schema\":\"ltfa.language-guide/v1\",\"cli_contract_version\":"
           << cli_contract_version() << ",\"tool_version\":";
    print_json_string(output, version_string());
    output << ",\"operation\":\"guide\",\"ok\":true,\"query\":{\"topic\":";
    if (page == nullptr) {
        output << "null,\"subtopic\":null";
    } else {
        print_json_string(output, page->topic);
        output << ",\"subtopic\":";
        if (page->subtopic.empty()) {
            output << "null";
        } else {
            print_json_string(output, page->subtopic);
        }
    }
    output << ",\"level\":";
    print_json_string(output, to_string(level));
    output << "},\"title\":";
    print_json_string(output, page == nullptr ? "LTFA Guide" : page->title);
    output << ",\"summary\":";
    print_json_string(output, page == nullptr
                                  ? "Bundled model-language, observation-data, and workflow "
                                    "guidance."
                                  : page->summary);
    output << ",\"domain\":";
    print_json_string(output, guide_domain(page));
    output << ",\"quick_reference\":";
    print_json_items(output, page == nullptr || level == GuideLevel::index ? std::string_view{}
                                                                           : page->quick_reference);
    output << ",\"syntax\":";
    print_json_items(output, page == nullptr || level == GuideLevel::index ? std::string_view{}
                                                                           : page->syntax);
    output << ",\"notes\":";
    print_json_items(output, page == nullptr || level != GuideLevel::detail ? std::string_view{}
                                                                            : page->notes);
    output << ",\"examples\":";
    print_json_items(output, page == nullptr || level != GuideLevel::detail ? std::string_view{}
                                                                            : page->examples);
    output << ",\"related\":";
    print_json_items(output, page == nullptr ? std::string_view{} : page->related);
    print_topic_index_json(output, "topics",
                           page == nullptr ? topics() : std::vector<const GuidePageDescriptor *>{},
                           false);
    print_topic_index_json(output, "subtopics",
                           page == nullptr ? std::vector<const GuidePageDescriptor *>{}
                                           : subtopics(page->topic),
                           true);
    print_observation_contract_json(output, page);
    output << ",\"navigation\":{\"guide_index\":\"ltfa guide\",\"command_help\":"
              "\"ltfa help guide\",\"language_reference\":"
              "\"share/doc/ltfa/docs/language/ltfa-dsl-v1.md\","
              "\"observation_reference\":";
    print_json_string(output, kObservationGuide.reference_path);
    output << ",\"diagnosis_sample\":";
    print_json_string(output, kObservationGuide.sample_path);
    output << "},\"diagnostics\":[]}\n";
}

} // namespace ltfa::app
