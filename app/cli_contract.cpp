// R: Render and query the canonical command catalogue for help and completion.
#include "cli_contract.hpp"
#include "language_guide.hpp"

#include "ltfa/version.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace ltfa::app {
namespace {

constexpr int kCliContractVersion = 5;

struct NavigationAction {
    std::string_view id;
    std::string_view kind;
    std::string description;
    std::string invocation;
};

constexpr std::array<ExitStatusDescriptor, 3> kHelpExitStatuses{{
    {0, "Successful help request."},
    {2, "CLI usage error."},
    {5, "Output failure."},
}};

constexpr std::array<ExitStatusDescriptor, 5> kModelExitStatuses{{
    {0, "Successful model operation."},
    {2, "CLI usage error."},
    {3, "Input, include, or parse failure."},
    {4, "Model validation failure."},
    {5, "Output failure."},
}};

constexpr std::array<ExitStatusDescriptor, 5> kAnalyzeExitStatuses{{
    {0, "Complete or bounded-incomplete assurance result."},
    {2, "CLI usage error."},
    {3, "Input, include, or parse failure."},
    {4, "Model or analysis-selection validation failure."},
    {5, "Output failure."},
}};

constexpr std::array<OperandDescriptor, 1> kHelpOperands{{
    {"command", "command-name", false, 0},
}};

constexpr std::array<OperandDescriptor, 2> kGuideOperands{{
    {"topic", "guide-topic", false, 0},
    {"subtopic", "guide-subtopic", false, 1},
}};

constexpr std::array<OptionDescriptor, 2> kHelpOptions{{
    {"help", "h", OptionKind::flag, "", "", false, false, "false", "", "all other arguments", "",
     "Print detailed help for the help command."},
    {"format", "", OptionKind::value, "format", "output-format", false, false, "text", "text|json",
     "", "", "Select human-readable or structured help output."},
}};

constexpr std::array<OptionDescriptor, 3> kGuideOptions{{
    {"help", "h", OptionKind::flag, "", "", false, false, "false", "", "all other arguments", "",
     "Print detailed help for the guide command."},
    {"level", "", OptionKind::value, "level", "guide-level", false, false,
     "index without a topic; quick with a topic; detail with a subtopic", "index|quick|detail", "",
     "", "Select index, quick, or detailed guidance."},
    {"format", "", OptionKind::value, "format", "output-format", false, false, "text", "text|json",
     "", "", "Select human-readable or structured language guidance."},
}};

constexpr std::array<OptionDescriptor, 6> kModelOptions{{
    {"help", "h", OptionKind::flag, "", "", false, false, "false", "", "all other arguments", "",
     "Print detailed help for the model command."},
    {"model-root", "", OptionKind::value, "directory", "local-directory", true, false, "", "", "",
     "", "Select the explicit local root for the entry file and includes."},
    {"input", "", OptionKind::value, "file", "relative-file", true, false, "", "", "", "",
     "Select the entry file relative to the model root."},
    {"format", "", OptionKind::value, "format", "output-format", false, false, "human",
     "human|json|dot", "", "", "Select human, JSON, or DOT model output."},
    {"dot-view", "", OptionKind::value, "view", "dot-view", false, false, "raw",
     "raw|topology|logical|protection|power|dependency|overlay", "", "format=dot",
     "Select the lossless raw or a semantic DOT relation view; valid only with --format dot."},
    {"include-restricted", "", OptionKind::flag, "", "", false, false, "false", "", "", "",
     "Explicitly include restricted model objects."},
}};

constexpr std::array<OptionDescriptor, 14> kAnalyzeOptions{{
    {"help", "h", OptionKind::flag, "", "", false, false, "false", "", "all other arguments", "",
     "Print detailed help for the analyze command."},
    {"model-root", "", OptionKind::value, "directory", "local-directory", true, false, "", "", "",
     "", "Select the explicit local root for the entry file and includes."},
    {"input", "", OptionKind::value, "file", "relative-file", true, false, "", "", "", "",
     "Select the entry file relative to the model root."},
    {"scope", "", OptionKind::value, "scope-id", "external-id", true, false, "", "", "", "",
     "Select one declared scope as the assurance view boundary."},
    {"source", "", OptionKind::value, "object-id", "external-id", true, false, "", "", "", "",
     "Select the source entity or endpoint inside the scope."},
    {"target", "", OptionKind::value, "object-id", "external-id", true, false, "", "", "", "",
     "Select a distinct target entity or endpoint inside the scope."},
    {"failure", "", OptionKind::value, "failure-id", "external-id", false, false, "", "", "", "",
     "Optionally propagate one declared failure mode."},
    {"format", "", OptionKind::value, "format", "output-format", false, false, "human",
     "human|json", "", "", "Select human or ltfa.assurance/v1 JSON output."},
    {"include-restricted", "", OptionKind::flag, "", "", false, false, "false", "", "", "",
     "Explicitly permit restricted stable IDs in assurance output."},
    {"max-candidates", "", OptionKind::value, "count", "positive-integer", false, false, "1024", "",
     "", "", "Bound the number of SPOF candidates."},
    {"max-visited-objects", "", OptionKind::value, "count", "positive-integer", false, false,
     "2000000", "", "", "", "Bound cumulative path and SPOF object work."},
    {"max-examined-relations", "", OptionKind::value, "count", "positive-integer", false, false,
     "10000000", "", "", "", "Bound cumulative path and SPOF relation work."},
    {"max-events", "", OptionKind::value, "count", "positive-integer", false, false, "1000000", "",
     "", "", "Bound scheduled propagation events."},
    {"max-state-transitions", "", OptionKind::value, "count", "positive-integer", false, false,
     "300000", "", "", "", "Bound propagation state transitions."},
}};

constexpr std::array<OptionDescriptor, 14> kCutsOptions{{
    {"help", "h", OptionKind::flag, "", "", false, false, "false", "", "all other arguments", "",
     "Print detailed help for the cuts command."},
    {"model-root", "", OptionKind::value, "directory", "local-directory", true, false, "", "", "",
     "", "Select the explicit local model root."},
    {"input", "", OptionKind::value, "file", "relative-file", true, false, "", "", "", "",
     "Select the entry file relative to the model root."},
    {"scope", "", OptionKind::value, "scope-id", "external-id", true, false, "", "", "", "",
     "Select one declared scope as the cut-analysis boundary."},
    {"source", "", OptionKind::value, "object-id", "external-id", true, false, "", "", "", "",
     "Select the source object."},
    {"target", "", OptionKind::value, "object-id", "external-id", true, false, "", "", "", "",
     "Select a distinct target object."},
    {"format", "", OptionKind::value, "format", "output-format", false, false, "human",
     "human|json", "", "", "Select human or ltfa.service-cuts/v1 JSON output."},
    {"include-restricted", "", OptionKind::flag, "", "", false, false, "false", "", "", "",
     "Explicitly permit restricted stable IDs."},
    {"max-cut-size", "", OptionKind::value, "count", "positive-integer", false, false, "2", "", "",
     "", "Bound cut cardinality to one or two."},
    {"max-candidate-sets", "", OptionKind::value, "count", "positive-integer", false, false,
     "10000", "", "", "", "Bound evaluated candidate sets."},
    {"max-results", "", OptionKind::value, "count", "positive-integer", false, false, "1024", "",
     "", "", "Bound returned cut findings."},
    {"max-visited-objects", "", OptionKind::value, "count", "positive-integer", false, false,
     "2000000", "", "", "", "Bound cumulative object traversal work."},
    {"max-examined-relations", "", OptionKind::value, "count", "positive-integer", false, false,
     "10000000", "", "", "", "Bound cumulative relation traversal work."},
    {"max-flow-transitions", "", OptionKind::value, "count", "positive-integer", false, false,
     "10000000", "", "", "", "Bound max-flow transitions."},
}};

constexpr std::array<OptionDescriptor, 12> kForwardingOptions{{
    {"help", "h", OptionKind::flag, "", "", false, false, "false", "", "all other arguments", "",
     "Print forwarding help."},
    {"model-root", "", OptionKind::value, "directory", "local-directory", true, false, "", "", "",
     "", "Select the explicit local model root."},
    {"input", "", OptionKind::value, "file", "relative-file", true, false, "", "", "", "",
     "Select the entry file relative to the root."},
    {"domain", "", OptionKind::value, "domain-id", "external-id", true, false, "", "", "", "",
     "Select one protection control-domain."},
    {"source", "", OptionKind::value, "object-id", "external-id", true, false, "", "", "", "",
     "Select a member device or its network endpoint."},
    {"failure", "", OptionKind::value, "failure-id", "external-id", false, true, "", "", "", "",
     "Simulate declared physical failures together; repeat for multiple faults."},
    {"format", "", OptionKind::value, "format", "output-format", false, false, "human",
     "human|json", "", "", "Select human or ltfa.forwarding/v1 JSON output."},
    {"include-restricted", "", OptionKind::flag, "", "", false, false, "false", "", "", "",
     "Explicitly permit restricted model IDs and provenance."},
    {"max-visited-objects", "", OptionKind::value, "count", "positive-integer", false, false,
     "100000", "", "", "", "Bound cumulative normal and post-failure object work."},
    {"max-examined-relations", "", OptionKind::value, "count", "positive-integer", false, false,
     "500000", "", "", "", "Bound cumulative normal and post-failure relation work."},
    {"max-events", "", OptionKind::value, "count", "positive-integer", false, false, "1000000", "",
     "", "", "Bound cumulative physical failure propagation events."},
    {"max-state-transitions", "", OptionKind::value, "count", "positive-integer", false, false,
     "300000", "", "", "", "Bound cumulative physical state transitions."},
}};

constexpr std::array<OptionDescriptor, 14> kDiagnoseOptions{{
    {"help", "h", OptionKind::flag, "", "", false, false, "false", "", "all other arguments", "",
     "Print detailed help for the diagnose command."},
    {"model-root", "", OptionKind::value, "directory", "local-directory", true, false, "", "", "",
     "", "Select the explicit local model root."},
    {"input", "", OptionKind::value, "file", "relative-file", true, false, "", "", "", "",
     "Select the model entry file."},
    {"observations", "", OptionKind::value, "file", "relative-file", true, false, "", "", "", "",
     "Read ltfa.observation/v1 JSON Lines below the model root."},
    {"analysis-at", "", OptionKind::value, "timestamp", "RFC3339-timestamp", true, false, "", "",
     "", "", "Set the explicit evidence reference time without reading a clock."},
    {"format", "", OptionKind::value, "format", "output-format", false, false, "human",
     "human|json|dot", "", "",
     "Select human, ltfa.diagnosis-cli/v1 JSON, or diagnosis DOT output."},
    {"include-restricted", "", OptionKind::flag, "", "", false, false, "false", "", "", "",
     "Explicitly permit restricted stable IDs."},
    {"max-causal-objects", "", OptionKind::value, "count", "positive-integer", false, false,
     "100000", "", "", "", "Bound causal-cone objects."},
    {"max-hypotheses", "", OptionKind::value, "count", "positive-integer", false, false, "10000",
     "", "", "", "Bound generated hypotheses."},
    {"max-simulations", "", OptionKind::value, "count", "positive-integer", false, false, "10000",
     "", "", "", "Bound failure simulations."},
    {"max-evidence-comparisons", "", OptionKind::value, "count", "positive-integer", false, false,
     "1000000", "", "", "", "Bound diagnosis evidence comparisons."},
    {"max-recommendation-comparisons", "", OptionKind::value, "count", "positive-integer", false,
     false, "1000000", "", "", "", "Bound next-observation comparisons."},
    {"max-candidate-probes", "", OptionKind::value, "count", "positive-integer", false, false,
     "10000", "", "", "", "Bound registered absent probes considered for recommendations."},
    {"max-recommendations", "", OptionKind::value, "count", "positive-integer", false, false,
     "1024", "", "", "", "Bound next-observation recommendations."},
}};

constexpr std::array<ExampleDescriptor, 5> kHelpExamples{{
    {"root", "ltfa help", "List the implemented command contract."},
    {"model", "ltfa help model", "Show detailed model command help."},
    {"analyze", "ltfa help analyze", "Show the offline assurance contract."},
    {"json", "ltfa help model --format json", "Discover the model command as JSON."},
    {"guide", "ltfa guide syntax", "Open quick LTFA DSL v1 syntax guidance."},
}};

constexpr std::array<ExampleDescriptor, 6> kGuideExamples{{
    {"index", "ltfa guide", "List bundled model-language, observation-data, and workflow topics."},
    {"syntax", "ltfa guide syntax", "Show quick document and lexical syntax."},
    {"observations", "ltfa guide observations", "Show quick observation JSON Lines guidance."},
    {"statement-detail", "ltfa guide statements relations --level detail",
     "Show detailed connection and relation syntax."},
    {"json", "ltfa guide samples minimal --format json",
     "Discover the installed minimal sample through ltfa.language-guide/v1."},
    {"diagnosis-sample", "ltfa guide samples diagnosis",
     "Discover the installed synthetic diagnosis example."},
}};

constexpr std::array<ExampleDescriptor, 4> kModelExamples{{
    {"human", "ltfa model --model-root topology --input main.ltfa",
     "Validate and summarize one local model."},
    {"json", "ltfa model --model-root topology --input main.ltfa --format json",
     "Emit a deterministic JSON model."},
    {"dot", "ltfa model --model-root topology --input main.ltfa --format dot",
     "Emit deterministic lossless raw DOT without invoking Graphviz."},
    {"semantic-dot",
     "ltfa model --model-root topology --input main.ltfa --format dot --dot-view topology",
     "Emit a deterministic semantic topology diagram."},
}};

constexpr std::array<ExampleDescriptor, 3> kAnalyzeExamples{{
    {"path",
     "ltfa analyze --model-root topology --input main.ltfa --scope site:a "
     "--source service:source --target service:target",
     "Analyze one scoped registered path and its qualified SPOF candidates."},
    {"failure",
     "ltfa analyze --model-root topology --input main.ltfa --scope site:a "
     "--source service:source --target service:target --failure failure:breaker",
     "Add one declared failure-propagation scenario."},
    {"json",
     "ltfa analyze --model-root topology --input main.ltfa --scope site:a "
     "--source service:source --target service:target --format json",
     "Emit deterministic ltfa.assurance/v1 JSON."},
}};

constexpr std::array<ExampleDescriptor, 2> kCutsExamples{{
    {"human",
     "ltfa cuts --model-root topology --input main.ltfa --scope site:a "
     "--source service:source --target service:target",
     "Find bounded service cuts for one scoped source and target."},
    {"json",
     "ltfa cuts --model-root topology --input main.ltfa --scope site:a "
     "--source service:source --target service:target --format json",
     "Emit deterministic ltfa.service-cuts/v1 JSON."},
}};

constexpr std::array<ExampleDescriptor, 2> kDiagnoseExamples{{
    {"human",
     "ltfa diagnose --model-root topology --input main.ltfa "
     "--observations observations.jsonl --analysis-at 2030-01-01T00:10:30Z",
     "Diagnose one bounded local observation file."},
    {"json",
     "ltfa diagnose --model-root topology --input main.ltfa "
     "--observations observations.jsonl --analysis-at 2030-01-01T00:10:30Z --format json",
     "Emit deterministic diagnosis and next-observation JSON."},
}};

constexpr std::array<ExampleDescriptor, 2> kForwardingExamples{{
    {"normal",
     "ltfa forwarding --model-root examples/turbo-ring --input main.ltfa --domain ring:main "
     "--source switch:1",
     "Inspect normal logical blocking and device reachability."},
    {"fault",
     "ltfa forwarding --model-root examples/turbo-ring --input main.ltfa --domain ring:main "
     "--source switch:1 --failure failure:link-1 --format json",
     "Inspect a declared link fault after convergence; recovery time remains unknown."},
}};

constexpr std::array<CommandDescriptor, 7> kCommands{{
    {"model", "model", "Load, validate, normalize, and render one local topology model.",
     "ltfa model --model-root <directory> --input <file> "
     "[--format human|json|dot] "
     "[--dot-view raw|topology|logical|protection|power|dependency|overlay] "
     "[--include-restricted]",
     CommandHandler::model, nullptr, 0, kModelOptions.data(), kModelOptions.size(),
     "local-model-tree", "human|json|dot", "model", "read",
     "ltfa.model/v1|ltfa.model.dot/v1|ltfa.diagram.dot/v1|ltfa.cli-error/v1",
     kModelExitStatuses.data(), kModelExitStatuses.size(), kModelExamples.data(),
     kModelExamples.size()},
    {"analyze", "analyze",
     "Run bounded path, qualified SPOF, and optional declared-failure analysis.",
     "ltfa analyze --model-root <directory> --input <file> --scope <scope-id> "
     "--source <object-id> --target <object-id> [--failure <failure-id>] "
     "[--format human|json] [--include-restricted] [--max-candidates <count>] "
     "[--max-visited-objects <count>] [--max-examined-relations <count>] "
     "[--max-events <count>] [--max-state-transitions <count>]",
     CommandHandler::analyze, nullptr, 0, kAnalyzeOptions.data(), kAnalyzeOptions.size(),
     "local-model-tree", "human|json", "assurance", "read", "ltfa.assurance/v1|ltfa.cli-error/v1",
     kAnalyzeExitStatuses.data(), kAnalyzeExitStatuses.size(), kAnalyzeExamples.data(),
     kAnalyzeExamples.size()},
    {"forwarding", "forwarding", "Evaluate normal and post-failure Turbo Ring forwarding offline.",
     "ltfa forwarding --model-root <directory> --input <file> --domain <domain-id> --source "
     "<object-id> "
     "[--failure <failure-id> ...] [--format human|json] [--include-restricted] [work budgets]",
     CommandHandler::forwarding, nullptr, 0, kForwardingOptions.data(), kForwardingOptions.size(),
     "local-model-tree", "human|json", "forwarding", "read", "ltfa.forwarding/v1|ltfa.cli-error/v1",
     kAnalyzeExitStatuses.data(), kAnalyzeExitStatuses.size(), kForwardingExamples.data(),
     kForwardingExamples.size()},
    {"cuts", "cuts", "Find bounded object, relation, and failure-domain service cuts.",
     "ltfa cuts --model-root <directory> --input <file> --scope <scope-id> --source <object-id> "
     "--target <object-id> [--format human|json] [--include-restricted] [work budgets]",
     CommandHandler::cuts, nullptr, 0, kCutsOptions.data(), kCutsOptions.size(), "local-model-tree",
     "human|json", "service-cuts", "read", "ltfa.service-cuts/v1|ltfa.cli-error/v1",
     kAnalyzeExitStatuses.data(), kAnalyzeExitStatuses.size(), kCutsExamples.data(),
     kCutsExamples.size()},
    {"diagnose", "diagnose", "Diagnose local observations and recommend the next useful probe.",
     "ltfa diagnose --model-root <directory> --input <file> --observations <file> "
     "--analysis-at <timestamp> [--format human|json|dot] [--include-restricted] [work budgets]",
     CommandHandler::diagnose, nullptr, 0, kDiagnoseOptions.data(), kDiagnoseOptions.size(),
     "local-model-tree+observation-jsonl", "human|json|dot", "diagnosis+recommendations", "read",
     "ltfa.diagnosis-cli/v1|ltfa.diagnosis.dot/v1|ltfa.cli-error/v1", kAnalyzeExitStatuses.data(),
     kAnalyzeExitStatuses.size(), kDiagnoseExamples.data(), kDiagnoseExamples.size()},
    {"help", "help", "Discover the implemented command contract.",
     "ltfa help [command] [--format text|json]", CommandHandler::help, kHelpOperands.data(),
     kHelpOperands.size(), kHelpOptions.data(), kHelpOptions.size(), "none", "text|json",
     "command-help", "read", "ltfa.command-help/v1|ltfa.cli-error/v1", kHelpExitStatuses.data(),
     kHelpExitStatuses.size(), kHelpExamples.data(), kHelpExamples.size()},
    {"guide", "guide", "Learn LTFA DSL v1, observation JSON Lines, and offline workflows.",
     "ltfa guide [topic] [subtopic] [--level index|quick|detail] [--format text|json]",
     CommandHandler::guide, kGuideOperands.data(), kGuideOperands.size(), kGuideOptions.data(),
     kGuideOptions.size(), "none", "text|json", "language-guide", "read",
     "ltfa.language-guide/v1|ltfa.cli-error/v1", kHelpExitStatuses.data(), kHelpExitStatuses.size(),
     kGuideExamples.data(), kGuideExamples.size()},
}};

std::vector<std::string_view> split_values(std::string_view values) {
    std::vector<std::string_view> result;
    std::size_t begin = 0;
    while (begin <= values.size()) {
        const auto end = values.find('|', begin);
        const auto length = end == std::string_view::npos ? values.size() - begin : end - begin;
        if (length != 0) {
            result.push_back(values.substr(begin, length));
        }
        if (end == std::string_view::npos) {
            break;
        }
        begin = end + 1;
    }
    return result;
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
                output << "\\u00" << kHexDigits[(byte >> 4U) & 0x0fU] << kHexDigits[byte & 0x0fU];
            } else {
                output << static_cast<char>(byte);
            }
            break;
        }
    }
    output << '"';
}

void print_nullable_json_string(std::ostream &output, std::string_view value) {
    if (value.empty()) {
        output << "null";
    } else {
        print_json_string(output, value);
    }
}

void print_json_string_array(std::ostream &output, std::string_view values) {
    output << '[';
    const auto entries = split_values(values);
    for (std::size_t index = 0; index < entries.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        print_json_string(output, entries[index]);
    }
    output << ']';
}

void print_help_option_json(std::ostream &output, const OptionDescriptor &option) {
    output << "{\"name\":";
    print_json_string(output, option.name);
    output << ",\"short\":";
    print_nullable_json_string(output, option.short_name);
    output << ",\"kind\":";
    print_json_string(output, option.kind == OptionKind::flag ? "flag" : "value");
    output << ",\"value_name\":";
    print_nullable_json_string(output, option.value_name);
    output << ",\"value_type\":";
    print_nullable_json_string(output, option.value_type);
    output << ",\"required\":" << (option.required ? "true" : "false")
           << ",\"repeatable\":" << (option.repeatable ? "true" : "false") << ",\"default\":";
    if (option.kind == OptionKind::flag) {
        output << (option.default_value == "true" ? "true" : "false");
    } else {
        print_nullable_json_string(output, option.default_value);
    }
    output << ",\"enum_values\":";
    print_json_string_array(output, option.enum_values);
    output << ",\"conflicts\":";
    print_json_string_array(output, option.conflicts);
    output << ",\"requires\":";
    print_json_string_array(output, option.required_options);
    output << ",\"description\":";
    print_json_string(output, option.description);
    output << '}';
}

std::string help_invocation(const CommandDescriptor *command);

std::string guide_invocation(const CommandDescriptor &command) {
    switch (command.handler) {
    case CommandHandler::model:
        return "ltfa guide syntax";
    case CommandHandler::analyze:
    case CommandHandler::cuts:
        return "ltfa guide workflows analyze";
    case CommandHandler::forwarding:
        return "ltfa guide workflows forwarding";
    case CommandHandler::diagnose:
        return "ltfa guide workflows diagnose";
    case CommandHandler::help:
    case CommandHandler::guide:
        return "ltfa guide";
    }
    return "ltfa guide";
}

void print_command_json(std::ostream &output, const CommandDescriptor &command) {
    output << "{\"cli_contract_version\":" << kCliContractVersion << ",\"path\":[";
    print_json_string(output, command.name);
    output << "],\"operation\":";
    print_json_string(output, command.operation);
    output << ",\"summary\":";
    print_json_string(output, command.summary);
    output << ",\"usage\":";
    print_json_string(output, command.usage);
    output << ",\"operands\":[";
    for (std::size_t index = 0; index < command.operand_count; ++index) {
        if (index != 0) {
            output << ',';
        }
        const auto &operand = command.operands[index];
        output << "{\"name\":";
        print_json_string(output, operand.name);
        output << ",\"value_type\":";
        print_json_string(output, operand.value_type);
        output << ",\"required\":" << (operand.required ? "true" : "false")
               << ",\"position\":" << operand.position << '}';
    }
    output << "],\"options\":[";
    for (std::size_t index = 0; index < command.option_count; ++index) {
        if (index != 0) {
            output << ',';
        }
        print_help_option_json(output, command.options[index]);
    }
    output << "],\"input\":";
    print_json_string(output, command.input);
    output << ",\"output\":{\"formats\":";
    print_json_string_array(output, command.output_formats);
    output << ",\"payload\":";
    print_json_string(output, command.output_payload);
    output << "},\"effect\":";
    print_json_string(output, command.effect);
    output << ",\"result_schemas\":";
    print_json_string_array(output, command.result_schemas);
    output << ",\"exit_statuses\":[";
    for (std::size_t index = 0; index < command.exit_status_count; ++index) {
        if (index != 0) {
            output << ',';
        }
        output << "{\"code\":" << command.exit_statuses[index].code << ",\"meaning\":";
        print_json_string(output, command.exit_statuses[index].meaning);
        output << '}';
    }
    output << "],\"examples\":[";
    for (std::size_t index = 0; index < command.example_count; ++index) {
        if (index != 0) {
            output << ',';
        }
        const auto &example = command.examples[index];
        output << "{\"id\":";
        print_json_string(output, example.id);
        output << ",\"invocation\":";
        print_json_string(output, example.invocation);
        output << ",\"summary\":";
        print_json_string(output, example.summary);
        output << '}';
    }
    output << "],\"navigation\":{\"help\":";
    print_json_string(output, help_invocation(&command));
    output << ",\"json_help\":";
    print_json_string(output, help_invocation(&command) + " --format json");
    output << ",\"language_guide\":";
    print_json_string(output, guide_invocation(command));
    if (command.handler == CommandHandler::diagnose) {
        const auto &observation = observation_guide_descriptor();
        output << ",\"observation_guide\":";
        print_json_string(output, observation.guide_invocation);
        output << ",\"observation_manual\":";
        print_json_string(output, observation.reference_path);
        output << ",\"diagnosis_sample\":";
        print_json_string(output, observation.sample_path);
    }
    output << "}}";
}

std::size_t restricted_damerau_levenshtein(std::string_view left, std::string_view right) {
    std::vector<std::vector<std::size_t>> distance(left.size() + 1,
                                                   std::vector<std::size_t>(right.size() + 1, 0));
    for (std::size_t left_index = 0; left_index <= left.size(); ++left_index) {
        distance[left_index][0] = left_index;
    }
    for (std::size_t right_index = 0; right_index <= right.size(); ++right_index) {
        distance[0][right_index] = right_index;
    }
    for (std::size_t left_index = 1; left_index <= left.size(); ++left_index) {
        for (std::size_t right_index = 1; right_index <= right.size(); ++right_index) {
            const std::size_t substitution =
                left[left_index - 1] == right[right_index - 1] ? 0U : 1U;
            auto value = std::min({distance[left_index - 1][right_index] + 1,
                                   distance[left_index][right_index - 1] + 1,
                                   distance[left_index - 1][right_index - 1] + substitution});
            if (left_index > 1 && right_index > 1 &&
                left[left_index - 1] == right[right_index - 2] &&
                left[left_index - 2] == right[right_index - 1]) {
                value = std::min(value, distance[left_index - 2][right_index - 2] + 1);
            }
            distance[left_index][right_index] = value;
        }
    }
    return distance[left.size()][right.size()];
}

template <typename Candidate>
std::string nearest_suggestion(std::string_view value, const Candidate &candidates) {
    std::string suggestion;
    auto best_distance = std::numeric_limits<std::size_t>::max();
    for (const auto candidate : candidates) {
        const auto distance = restricted_damerau_levenshtein(value, candidate);
        const auto limit = std::max<std::size_t>(1, std::max(value.size(), candidate.size()) / 3);
        if (distance <= limit && distance < best_distance) {
            suggestion = candidate;
            best_distance = distance;
        }
    }
    return suggestion;
}

std::string help_invocation(const CommandDescriptor *command) {
    if (command == nullptr) {
        return "ltfa help";
    }
    return "ltfa help " + std::string{command->name};
}

std::string retry_invocation(const CommandDescriptor *command, const UsageError *usage) {
    if (usage != nullptr && usage->operation == "help") {
        std::string invocation{"ltfa help"};
        if (!usage->suggestion.empty()) {
            invocation += ' ';
            invocation += usage->suggestion;
        }
        return invocation;
    }
    if (command != nullptr) {
        return "ltfa " + std::string{command->name};
    }
    if (usage != nullptr && usage->kind == "unknown_command" && !usage->suggestion.empty()) {
        return "ltfa " + usage->suggestion;
    }
    return "ltfa";
}

std::string diagnostic_guide_invocation(const UsageError *usage, const CliDiagnostic &diagnostic) {
    if (usage != nullptr && !usage->guide_target.empty()) {
        return usage->guide_target;
    }
    if (diagnostic.code.rfind("E_DSL_", 0) == 0) {
        return "ltfa guide errors dsl";
    }
    if (diagnostic.code.rfind("E_INCLUDE_", 0) == 0) {
        return "ltfa guide errors include";
    }
    if (diagnostic.code.rfind("LTOBS-", 0) == 0) {
        return std::string{observation_guide_descriptor().error_guide_invocation};
    }
    if (diagnostic.code.rfind("E_", 0) == 0) {
        return "ltfa guide errors validation";
    }
    return {};
}

std::vector<NavigationAction> action_navigation(const CommandDescriptor *help_target,
                                                const UsageError *usage,
                                                const CliDiagnostic &diagnostic) {
    const auto retry = retry_invocation(help_target, usage);
    std::string description;
    if (usage != nullptr && usage->kind == "missing_command") {
        description = "Choose a command, then rerun " + retry + ".";
    } else if (usage != nullptr && !usage->suggestion.empty()) {
        description = "Replace " + usage->token + " with " + usage->suggestion + ", then rerun " +
                      retry + ".";
    } else if (usage != nullptr) {
        description = "Correct the reported command usage, then rerun " + retry + ".";
    } else if (diagnostic.code.rfind("E_", 0) == 0 || diagnostic.code.rfind("LTOBS-", 0) == 0) {
        description = "Correct the reported local input, then rerun " + retry + ".";
    } else {
        description = "Correct the reported selection, then rerun " + retry + ".";
    }

    std::string help_description;
    if (help_target == nullptr) {
        help_description = "Inspect the root command contract.";
    } else {
        help_description = "Inspect the " + std::string{help_target->name} + " command contract.";
    }
    std::vector<NavigationAction> result;
    result.push_back({"correct-and-retry", "instruction", std::move(description), ""});
    const auto guide = diagnostic_guide_invocation(usage, diagnostic);
    if (!guide.empty()) {
        result.push_back({"inspect-guide", "command", "Inspect the applicable LTFA Guide.", guide});
    }
    result.push_back(
        {"inspect-help", "command", std::move(help_description), help_invocation(help_target)});
    return result;
}

void print_navigation_action_json(std::ostream &output, const NavigationAction &action) {
    output << "{\"id\":";
    print_json_string(output, action.id);
    output << ",\"kind\":";
    print_json_string(output, action.kind);
    output << ",\"description\":";
    print_json_string(output, action.description);
    output << ",\"invocation\":";
    print_nullable_json_string(output, action.invocation);
    output << '}';
}

void print_source_json(std::ostream &output, const SourceLocation &source) {
    output << "{\"file\":";
    print_json_string(output, source.file);
    output << ",\"line\":" << source.line << ",\"column\":" << source.column << '}';
}

} // namespace

CommandCatalog command_catalog() { return {kCommands.data(), kCommands.size()}; }

int cli_contract_version() noexcept { return kCliContractVersion; }

const CommandDescriptor *find_command(std::string_view name) {
    const auto catalog = command_catalog();
    for (std::size_t index = 0; index < catalog.command_count; ++index) {
        if (catalog.commands[index].name == name) {
            return &catalog.commands[index];
        }
    }
    return nullptr;
}

const OptionDescriptor *find_option(const CommandDescriptor &command, std::string_view name,
                                    bool short_name) {
    for (std::size_t index = 0; index < command.option_count; ++index) {
        const auto &option = command.options[index];
        if ((!short_name && option.name == name) ||
            (short_name && !option.short_name.empty() && option.short_name == name)) {
            return &option;
        }
    }
    return nullptr;
}

std::string suggest_command(std::string_view name) {
    if (name.size() > 128) {
        return {};
    }
    std::array<std::string_view, kCommands.size()> candidates{};
    for (std::size_t index = 0; index < kCommands.size(); ++index) {
        candidates[index] = kCommands[index].name;
    }
    return nearest_suggestion(name, candidates);
}

std::string suggest_option(const CommandDescriptor &command, std::string_view name) {
    if (name.size() > 128) {
        return {};
    }
    std::vector<std::string_view> candidates;
    candidates.reserve(command.option_count);
    for (std::size_t index = 0; index < command.option_count; ++index) {
        candidates.push_back(command.options[index].name);
    }
    return nearest_suggestion(name, candidates);
}

bool enum_contains(std::string_view values, std::string_view value) {
    if (values.empty()) {
        return true;
    }
    const auto entries = split_values(values);
    return std::find(entries.begin(), entries.end(), value) != entries.end();
}

std::string render_enum_values(std::string_view values) {
    const auto entries = split_values(values);
    std::string result;
    for (std::size_t index = 0; index < entries.size(); ++index) {
        if (index != 0) {
            result += index + 1 == entries.size() ? ", or " : ", ";
        }
        result += entries[index];
    }
    return result;
}

bool json_requested(int argc, char *argv[]) {
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--format=json") {
            return true;
        }
        if (argument == "--format" && index + 1 < argc &&
            std::string_view{argv[index + 1]} == "json") {
            return true;
        }
    }
    return false;
}

void print_root_help(std::ostream &output) {
    output << "LTFA - lightweight multilayer topology and fault analyzer\n"
           << "\n"
           << "Usage:\n";
    const auto catalog = command_catalog();
    for (std::size_t index = 0; index < catalog.command_count; ++index) {
        output << "  " << catalog.commands[index].usage << '\n';
    }
    output << "  ltfa --help\n"
           << "  ltfa --version\n"
           << "\n"
           << "Commands:\n";
    for (std::size_t index = 0; index < catalog.command_count; ++index) {
        output << "  " << catalog.commands[index].name << "  " << catalog.commands[index].summary
               << '\n';
    }
    output << "\n"
           << "Discovery:\n"
           << "  ltfa help <command>\n"
           << "  ltfa help <command> --format json\n"
           << "\n"
           << "Guides:\n"
           << "  ltfa guide\n"
           << "  ltfa guide syntax\n"
           << "  ltfa guide observations\n"
           << "  ltfa guide <topic> <subtopic> --level detail\n"
           << "\n"
           << "Bash completion:\n"
           << "  source completions/ltfa.bash\n"
           << "  Installed packages place ltfa in the standard bash-completion directory.\n"
           << "\n"
           << "The current core is offline and does not execute probes, commands, or network "
              "operations.\n";
}

void print_command_help(std::ostream &output, const CommandDescriptor &command) {
    output << "Command: ltfa " << command.name << '\n'
           << "Summary: " << command.summary << '\n'
           << "Operation: " << command.operation << '\n'
           << "CLI contract: " << kCliContractVersion << '\n'
           << "Usage:\n"
           << "  " << command.usage << '\n'
           << "Operands:\n";
    if (command.operand_count == 0) {
        output << "  (none)\n";
    } else {
        for (std::size_t index = 0; index < command.operand_count; ++index) {
            const auto &operand = command.operands[index];
            output << "  " << operand.position << ": " << operand.name
                   << " type=" << operand.value_type
                   << " required=" << (operand.required ? "true" : "false") << '\n';
        }
    }
    output << "Options:\n";
    for (std::size_t index = 0; index < command.option_count; ++index) {
        const auto &option = command.options[index];
        output << "  ";
        if (!option.short_name.empty()) {
            output << '-' << option.short_name << ", ";
        } else {
            output << "    ";
        }
        output << "--" << option.name;
        if (option.kind == OptionKind::value) {
            output << " <" << option.value_name << '>';
        }
        output << '\n'
               << "    required=" << (option.required ? "true" : "false")
               << " repeatable=" << (option.repeatable ? "true" : "false");
        if (!option.default_value.empty()) {
            output << " default=" << option.default_value;
        }
        output << '\n';
        if (!option.enum_values.empty()) {
            output << "    enum=" << option.enum_values << '\n';
        }
        if (!option.conflicts.empty()) {
            output << "    conflicts=" << option.conflicts << '\n';
        }
        if (!option.required_options.empty()) {
            output << "    requires=" << option.required_options << '\n';
        }
        output << "    " << option.description << '\n';
    }
    output << "Input: " << command.input << '\n'
           << "Effect: " << command.effect << '\n'
           << "Output: formats=" << command.output_formats << " payload=" << command.output_payload
           << '\n'
           << "Result schemas: " << command.result_schemas << '\n'
           << "Exit statuses:\n";
    for (std::size_t index = 0; index < command.exit_status_count; ++index) {
        output << "  " << command.exit_statuses[index].code << ": "
               << command.exit_statuses[index].meaning << '\n';
    }
    output << "Examples:\n";
    for (std::size_t index = 0; index < command.example_count; ++index) {
        output << "  " << command.examples[index].id << ": " << command.examples[index].invocation
               << '\n'
               << "    " << command.examples[index].summary << '\n';
    }
    output << "Navigation:\n"
           << "  help: " << help_invocation(&command) << '\n'
           << "  json-help: " << help_invocation(&command) << " --format json\n"
           << "  language-guide: " << guide_invocation(command) << '\n';
    if (command.handler == CommandHandler::diagnose) {
        const auto &observation = observation_guide_descriptor();
        output << "  observation-guide: " << observation.guide_invocation << '\n'
               << "  observation-manual: " << observation.reference_path << '\n'
               << "  diagnosis-sample: " << observation.sample_path << '\n';
    }
    output << "  root-help: ltfa help\n";
}

void print_help_json(std::ostream &output, const CommandDescriptor *command) {
    output << "{\"schema\":\"ltfa.command-help/v1\",\"cli_contract_version\":"
           << kCliContractVersion << ",\"tool_version\":";
    print_json_string(output, version_string());
    output << ",\"operation\":\"help\",\"ok\":true,\"query\":{\"command\":";
    if (command == nullptr) {
        output << "null";
    } else {
        print_json_string(output, command->name);
    }
    output << "},\"commands\":[";
    if (command == nullptr) {
        const auto catalog = command_catalog();
        for (std::size_t index = 0; index < catalog.command_count; ++index) {
            if (index != 0) {
                output << ',';
            }
            print_command_json(output, catalog.commands[index]);
        }
    } else {
        print_command_json(output, *command);
    }
    output << "],\"navigation\":{\"root_help\":\"ltfa help\","
              "\"command_help\":\"ltfa help <command>\","
              "\"json_help\":\"ltfa help <command> --format json\","
              "\"language_guide\":\"ltfa guide\","
              "\"bash_completion\":{\"repository_script\":\"completions/ltfa.bash\","
              "\"installed_location\":\"share/bash-completion/completions/ltfa\"}},"
              "\"diagnostics\":[]}\n";
}

void print_usage_error(std::ostream &error, const UsageError &usage) {
    error << "LTCLI-001 error: " << usage.message << '\n';
    if (!usage.token.empty()) {
        error << "  token: " << usage.token << '\n';
    }
    if (!usage.suggestion.empty()) {
        error << "  suggestion: " << usage.suggestion << '\n';
    }
    CliDiagnostic diagnostic;
    diagnostic.code = "LTCLI-001";
    diagnostic.severity = "error";
    diagnostic.message = usage.message;
    print_error_navigation(error, usage.help_target, &usage, diagnostic);
}

void print_error_navigation(std::ostream &error, const CommandDescriptor *help_target,
                            const UsageError *usage, const CliDiagnostic &diagnostic) {
    const auto actions = action_navigation(help_target, usage, diagnostic);
    error << "  next: " << actions.front().description << '\n';
    for (std::size_t index = 1; index < actions.size(); ++index) {
        if (actions[index].id == "inspect-guide") {
            error << "  guide: " << actions[index].invocation << '\n';
        } else if (actions[index].id == "inspect-help") {
            error << "  help: " << actions[index].invocation << '\n';
        }
    }
}

void print_cli_error_json(std::ostream &output, int exit_status, std::string_view operation,
                          const CommandDescriptor *help_target, const UsageError *usage,
                          const CliDiagnostic &diagnostic) {
    output << "{\"schema\":\"ltfa.cli-error/v1\",\"cli_contract_version\":" << kCliContractVersion
           << ",\"tool_version\":";
    print_json_string(output, version_string());
    output << ",\"operation\":";
    print_nullable_json_string(output, operation);
    output << ",\"ok\":false,\"exit_status\":" << exit_status << ",\"help_target\":{\"command\":";
    if (help_target == nullptr) {
        output << "null";
    } else {
        print_json_string(output, help_target->name);
    }
    output << ",\"invocation\":";
    print_json_string(output, help_invocation(help_target));
    output << "},\"usage\":";
    if (usage == nullptr) {
        output << "null";
    } else {
        output << "{\"kind\":";
        print_json_string(output, usage->kind);
        output << ",\"token\":";
        print_nullable_json_string(output, usage->token);
        output << ",\"suggestion\":";
        print_nullable_json_string(output, usage->suggestion);
        output << '}';
    }
    output << ",\"actions\":[";
    const auto actions = action_navigation(help_target, usage, diagnostic);
    for (std::size_t index = 0; index < actions.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        print_navigation_action_json(output, actions[index]);
    }
    output << "],\"diagnostics\":[{\"code\":";
    print_json_string(output, diagnostic.code);
    output << ",\"severity\":";
    print_json_string(output, diagnostic.severity);
    output << ",\"message\":";
    print_json_string(output, diagnostic.message);
    output << ",\"source\":";
    if (diagnostic.source.has_value()) {
        print_source_json(output, *diagnostic.source);
    } else {
        output << "null";
    }
    output << ",\"related\":[";
    for (std::size_t index = 0; index < diagnostic.related.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        output << "{\"source\":";
        print_source_json(output, diagnostic.related[index].source);
        output << ",\"message\":";
        print_json_string(output, diagnostic.related[index].message);
        output << '}';
    }
    output << "],\"data\":";
    if (usage == nullptr) {
        output << "{}";
    } else {
        output << "{\"usage_kind\":";
        print_json_string(output, usage->kind);
        output << ",\"token\":";
        print_nullable_json_string(output, usage->token);
        output << ",\"suggestion\":";
        print_nullable_json_string(output, usage->suggestion);
        output << '}';
    }
    output << "}]}\n";
}

void print_bash_completion_query(std::ostream &output, int argc, char *argv[], int begin) {
    std::vector<std::string_view> words;
    for (int index = begin; index < argc; ++index) {
        words.emplace_back(argv[index]);
    }
    const std::string_view current = words.empty() ? std::string_view{} : words.back();
    const auto matches_current = [current](std::string_view candidate) {
        return current.empty() || (candidate.size() >= current.size() &&
                                   candidate.substr(0, current.size()) == current);
    };
    const auto print_candidate = [&output, &matches_current](std::string_view candidate) {
        if (matches_current(candidate)) {
            output << candidate << '\n';
        }
    };
    const auto print_mode = [&output](std::string_view mode) {
        output << "__ltfa_completion_mode=" << mode << '\n';
    };
    const auto print_commands = [&print_candidate]() {
        const auto catalog = command_catalog();
        for (std::size_t index = 0; index < catalog.command_count; ++index) {
            print_candidate(catalog.commands[index].name);
        }
    };

    if (words.size() <= 1) {
        print_mode("plain");
        print_commands();
        print_candidate("--help");
        print_candidate("-h");
        print_candidate("--version");
        return;
    }

    const auto *command = find_command(words.front());
    if (command == nullptr) {
        print_mode("none");
        return;
    }

    const auto option_for_token = [command](std::string_view token) {
        if (token.size() > 2 && token.substr(0, 2) == "--") {
            const auto equals = token.find('=');
            const auto name = token.substr(
                2, equals == std::string_view::npos ? std::string_view::npos : equals - 2);
            return find_option(*command, name);
        }
        if (token.size() == 2 && token.front() == '-') {
            return find_option(*command, token.substr(1), true);
        }
        return static_cast<const OptionDescriptor *>(nullptr);
    };
    const auto print_option_value = [&print_mode,
                                     &print_candidate](const OptionDescriptor &option) {
        if (!option.enum_values.empty()) {
            print_mode("plain");
            for (const auto value : split_values(option.enum_values)) {
                print_candidate(value);
            }
        } else if (option.value_type == "local-directory") {
            print_mode("dir");
        } else if (option.value_type == "relative-file") {
            print_mode("file");
        } else {
            print_mode("none");
        }
    };

    const auto previous = words[words.size() - 2];
    if (const auto *option = option_for_token(previous);
        option != nullptr && option->kind == OptionKind::value &&
        previous.find('=') == std::string_view::npos) {
        print_option_value(*option);
        return;
    }

    if (current.size() > 3 && current.substr(0, 2) == "--") {
        const auto equals = current.find('=');
        if (equals != std::string_view::npos) {
            const auto *option = find_option(*command, current.substr(2, equals - 2));
            if (option == nullptr || option->kind != OptionKind::value ||
                option->enum_values.empty()) {
                print_mode("none");
                return;
            }
            print_mode("plain");
            const auto value_prefix = current.substr(equals + 1);
            for (const auto value : split_values(option->enum_values)) {
                if (value.size() >= value_prefix.size() &&
                    value.substr(0, value_prefix.size()) == value_prefix) {
                    const auto candidate =
                        std::string{current.substr(0, equals + 1)} + std::string{value};
                    output << candidate << '\n';
                }
            }
            return;
        }
    }

    std::vector<const OptionDescriptor *> used_options;
    std::vector<std::string_view> operands;
    bool options_ended = false;
    for (std::size_t index = 1; index + 1 < words.size(); ++index) {
        const auto token = words[index];
        if (!options_ended && token == "--") {
            options_ended = true;
            continue;
        }
        const auto *option = options_ended ? nullptr : option_for_token(token);
        if (option == nullptr) {
            operands.push_back(token);
            continue;
        }
        used_options.push_back(option);
        if (option->kind == OptionKind::value && token.find('=') == std::string_view::npos &&
            index + 2 < words.size()) {
            ++index;
        }
    }

    if (options_ended) {
        print_mode("none");
        return;
    }
    const auto option_used = [&used_options](const OptionDescriptor &option) {
        return std::find(used_options.begin(), used_options.end(), &option) != used_options.end();
    };
    const auto *help_option = find_option(*command, "help");
    if (help_option != nullptr && option_used(*help_option)) {
        print_mode("none");
        return;
    }

    if (command->handler == CommandHandler::help && operands.empty() &&
        (current.empty() || current.front() != '-')) {
        print_mode("plain");
        print_commands();
        return;
    }

    if (command->handler == CommandHandler::guide && (current.empty() || current.front() != '-')) {
        print_mode("plain");
        const auto catalog = guide_catalog();
        if (operands.empty()) {
            for (std::size_t index = 0; index < catalog.page_count; ++index) {
                if (catalog.pages[index].subtopic.empty()) {
                    print_candidate(catalog.pages[index].topic);
                }
            }
            return;
        }
        if (operands.size() == 1) {
            for (std::size_t index = 0; index < catalog.page_count; ++index) {
                if (catalog.pages[index].topic == operands.front() &&
                    !catalog.pages[index].subtopic.empty()) {
                    print_candidate(catalog.pages[index].subtopic);
                }
            }
            return;
        }
        return;
    }

    if (!current.empty() && current.front() != '-') {
        print_mode("none");
        return;
    }

    print_mode("plain");
    for (std::size_t index = 0; index < command->option_count; ++index) {
        const auto &option = command->options[index];
        if (option_used(option) && !option.repeatable) {
            continue;
        }
        const auto long_name = "--" + std::string{option.name};
        print_candidate(long_name);
        if (!option.short_name.empty()) {
            const auto short_name = "-" + std::string{option.short_name};
            print_candidate(short_name);
        }
    }
}

} // namespace ltfa::app
