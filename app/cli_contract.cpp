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

constexpr int kCliContractVersion = 2;

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

constexpr std::array<OptionDescriptor, 2> kHelpOptions{{
    {"help", "h", OptionKind::flag, "", "", false, false, "false", "", "all other arguments", "",
     "Print detailed help for the help command."},
    {"format", "", OptionKind::value, "format", "output-format", false, false, "text", "text|json",
     "", "", "Select human-readable or structured help output."},
}};

constexpr std::array<OptionDescriptor, 5> kModelOptions{{
    {"help", "h", OptionKind::flag, "", "", false, false, "false", "", "all other arguments", "",
     "Print detailed help for the model command."},
    {"model-root", "", OptionKind::value, "directory", "local-directory", true, false, "", "", "",
     "", "Select the explicit local root for the entry file and includes."},
    {"input", "", OptionKind::value, "file", "relative-file", true, false, "", "", "", "",
     "Select the entry file relative to the model root."},
    {"format", "", OptionKind::value, "format", "output-format", false, false, "human",
     "human|json|dot", "", "", "Select human, JSON, or DOT model output."},
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

constexpr std::array<ExampleDescriptor, 4> kHelpExamples{{
    {"root", "ltfa help", "List the implemented command contract."},
    {"model", "ltfa help model", "Show detailed model command help."},
    {"analyze", "ltfa help analyze", "Show the offline assurance contract."},
    {"json", "ltfa help model --format json", "Discover the model command as JSON."},
}};

constexpr std::array<ExampleDescriptor, 3> kModelExamples{{
    {"human", "ltfa model --model-root topology --input main.ltfa",
     "Validate and summarize one local model."},
    {"json", "ltfa model --model-root topology --input main.ltfa --format json",
     "Emit a deterministic JSON model."},
    {"dot", "ltfa model --model-root topology --input main.ltfa --format dot",
     "Emit deterministic DOT without invoking Graphviz."},
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

constexpr std::array<CommandDescriptor, 3> kCommands{{
    {"model", "model", "Load, validate, normalize, and render one local topology model.",
     "ltfa model --model-root <directory> --input <file> "
     "[--format human|json|dot] [--include-restricted]",
     CommandHandler::model, nullptr, 0, kModelOptions.data(), kModelOptions.size(),
     "local-model-tree", "human|json|dot", "model", "read", "ltfa.model/v1|ltfa.cli-error/v1",
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
    {"help", "help", "Discover the implemented command contract.",
     "ltfa help [command] [--format text|json]", CommandHandler::help, kHelpOperands.data(),
     kHelpOperands.size(), kHelpOptions.data(), kHelpOptions.size(), "none", "text|json",
     "command-help", "read", "ltfa.command-help/v1|ltfa.cli-error/v1", kHelpExitStatuses.data(),
     kHelpExitStatuses.size(), kHelpExamples.data(), kHelpExamples.size()},
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
    output << "]}";
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

void print_source_json(std::ostream &output, const SourceLocation &source) {
    output << "{\"file\":";
    print_json_string(output, source.file);
    output << ",\"line\":" << source.line << ",\"column\":" << source.column << '}';
}

} // namespace

CommandCatalog command_catalog() { return {kCommands.data(), kCommands.size()}; }

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
    output << "],\"diagnostics\":[]}\n";
}

void print_usage_error(std::ostream &error, const UsageError &usage) {
    error << "LTCLI-001 error: " << usage.message << '\n';
    if (!usage.token.empty()) {
        error << "  token: " << usage.token << '\n';
    }
    if (!usage.suggestion.empty()) {
        error << "  suggestion: " << usage.suggestion << '\n';
    }
    error << "  help: " << help_invocation(usage.help_target) << '\n';
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
    output << ",\"diagnostics\":[{\"code\":";
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

} // namespace ltfa::app
