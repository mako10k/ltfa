#include "cli.hpp"

#include "cli_contract.hpp"

#include "ltfa/dsl/include_resolver.hpp"
#include "ltfa/error.hpp"
#include "ltfa/model_compiler.hpp"
#include "ltfa/model_output.hpp"
#include "ltfa/version.hpp"

#include <filesystem>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ltfa::app {
namespace {

namespace fs = std::filesystem;

constexpr int kExitSuccess = 0;
constexpr int kExitUsage = 2;
constexpr int kExitInput = 3;
constexpr int kExitValidation = 4;
constexpr int kExitOutput = 5;

enum class OutputFormat {
    human,
    json,
    dot,
};

struct ParsedOption {
    const OptionDescriptor *descriptor;
    std::string_view value;
};

struct ParsedArguments {
    std::vector<std::string_view> operands;
    std::vector<ParsedOption> options;

    [[nodiscard]] bool has_option(std::string_view name) const {
        for (const auto &option : options) {
            if (option.descriptor->name == name) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] std::optional<std::string_view> option_value(std::string_view name) const {
        for (const auto &option : options) {
            if (option.descriptor->name == name) {
                return option.value;
            }
        }
        return std::nullopt;
    }
};

struct ModelOptions {
    fs::path model_root;
    fs::path input;
    OutputFormat format{OutputFormat::human};
    bool include_restricted{false};
};

UsageError make_usage_error(std::string kind, std::string message, std::string token,
                            const CommandDescriptor *help_target, std::string suggestion = {}) {
    UsageError usage;
    usage.kind = std::move(kind);
    usage.message = std::move(message);
    usage.operation = help_target == nullptr ? "" : std::string{help_target->operation};
    usage.token = std::move(token);
    usage.suggestion = std::move(suggestion);
    usage.help_target = help_target;
    return usage;
}

int emit_usage_error(std::ostream &output, std::ostream &error, const UsageError &usage,
                     bool json) {
    if (json) {
        CliDiagnostic diagnostic;
        diagnostic.code = "LTCLI-001";
        diagnostic.severity = "error";
        diagnostic.message = usage.message;
        print_cli_error_json(output, kExitUsage, usage.operation, usage.help_target, &usage,
                             diagnostic);
        if (!output) {
            error << "ltfa: failed to write error output\n";
            return kExitOutput;
        }
    } else {
        print_usage_error(error, usage);
        if (!error) {
            return kExitOutput;
        }
    }
    return kExitUsage;
}

int finish_output(std::ostream &output, std::ostream &error, std::string_view kind) {
    if (output) {
        return kExitSuccess;
    }
    error << "ltfa: failed to write " << kind << " output\n";
    return kExitOutput;
}

bool parse_command_arguments(const CommandDescriptor &command, int argc, char *argv[], int begin,
                             ParsedArguments &parsed, UsageError &usage) {
    bool options_ended = false;
    for (int index = begin; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (!options_ended && argument == "--") {
            options_ended = true;
            continue;
        }
        if (options_ended || argument.empty() || argument.front() != '-') {
            parsed.operands.push_back(argument);
            continue;
        }

        const OptionDescriptor *descriptor = nullptr;
        std::string_view option_name;
        std::string_view inline_value;
        bool has_inline_value = false;
        if (argument.size() > 2 && argument.substr(0, 2) == "--") {
            const auto equals = argument.find('=');
            option_name = argument.substr(
                2, equals == std::string_view::npos ? std::string_view::npos : equals - 2);
            has_inline_value = equals != std::string_view::npos;
            if (has_inline_value) {
                inline_value = argument.substr(equals + 1);
            }
            descriptor = find_option(command, option_name);
        } else if (argument.size() == 2) {
            option_name = argument.substr(1);
            descriptor = find_option(command, option_name, true);
        }

        if (descriptor == nullptr) {
            std::string suggestion;
            if (argument.size() > 2 && argument.substr(0, 2) == "--") {
                const auto nearest = suggest_option(command, option_name);
                if (!nearest.empty()) {
                    suggestion = "--" + nearest;
                }
            }
            usage = make_usage_error("unknown_option", "unknown option " + std::string{argument},
                                     std::string{argument}, &command, std::move(suggestion));
            return false;
        }

        if (parsed.has_option(descriptor->name) && !descriptor->repeatable) {
            usage =
                make_usage_error("duplicate_option",
                                 "option --" + std::string{descriptor->name} + " is not repeatable",
                                 std::string{argument}, &command);
            return false;
        }

        std::string_view value;
        if (descriptor->kind == OptionKind::flag) {
            if (has_inline_value) {
                usage = make_usage_error("unexpected_option_value",
                                         "boolean option --" + std::string{descriptor->name} +
                                             " does not take a value",
                                         std::string{argument}, &command);
                return false;
            }
        } else if (has_inline_value) {
            value = inline_value;
        } else {
            const bool next_is_option = index + 1 < argc &&
                                        std::string_view{argv[index + 1]}.size() > 1 &&
                                        std::string_view{argv[index + 1]}.front() == '-';
            if (index + 1 >= argc || next_is_option) {
                usage = make_usage_error("missing_option_value",
                                         "option --" + std::string{descriptor->name} +
                                             " requires a value",
                                         std::string{argument}, &command);
                return false;
            }
            value = argv[++index];
        }

        if (descriptor->kind == OptionKind::value &&
            !enum_contains(descriptor->enum_values, value)) {
            usage =
                make_usage_error("invalid_option_value",
                                 "option --" + std::string{descriptor->name} + " must be one of " +
                                     render_enum_values(descriptor->enum_values),
                                 std::string{value}, &command);
            return false;
        }
        parsed.options.push_back({descriptor, value});
    }

    if (parsed.has_option("help")) {
        if (parsed.options.size() != 1 || !parsed.operands.empty()) {
            usage =
                make_usage_error("option_conflict", "option --help conflicts with other arguments",
                                 "--help", &command);
            return false;
        }
        return true;
    }

    if (parsed.operands.size() > command.operand_count) {
        const auto operand = parsed.operands[command.operand_count];
        usage = make_usage_error("extra_operand", "unexpected operand " + std::string{operand},
                                 std::string{operand}, &command);
        return false;
    }
    for (std::size_t index = 0; index < command.operand_count; ++index) {
        if (command.operands[index].required && index >= parsed.operands.size()) {
            usage = make_usage_error("missing_operand",
                                     "missing operand <" +
                                         std::string{command.operands[index].name} + ">",
                                     "", &command);
            return false;
        }
    }
    for (std::size_t index = 0; index < command.option_count; ++index) {
        const auto &option = command.options[index];
        if (option.required && !parsed.has_option(option.name)) {
            usage = make_usage_error("missing_required_option",
                                     "option --" + std::string{option.name} + " is required", "",
                                     &command);
            return false;
        }
    }
    return true;
}

std::string display_source(StringId file, const std::vector<dsl::LoadedSource> &sources,
                           const fs::path &canonical_root) {
    if (!file.is_valid()) {
        return "<model>";
    }
    for (const auto &source : sources) {
        if (source.id != file) {
            continue;
        }
        const auto relative = source.canonical_path.lexically_relative(canonical_root);
        if (!relative.empty() && !relative.is_absolute()) {
            const auto first = relative.begin();
            if (first == relative.end() || *first != "..") {
                return relative.generic_string();
            }
        }
        return source.canonical_path.generic_string();
    }
    return "<model>";
}

void print_diagnostic(std::ostream &error, const Diagnostic &diagnostic,
                      const std::vector<dsl::LoadedSource> &sources,
                      const fs::path &canonical_root) {
    error << display_source(diagnostic.source.file, sources, canonical_root) << ':'
          << diagnostic.source.begin.line << ':' << diagnostic.source.begin.column << ": "
          << to_string(diagnostic.severity) << ' ' << to_string(diagnostic.code) << ": "
          << diagnostic.message << '\n';
    if (diagnostic.secondary_source.has_value()) {
        error << "  note: "
              << display_source(diagnostic.secondary_source->file, sources, canonical_root) << ':'
              << diagnostic.secondary_source->begin.line << ':'
              << diagnostic.secondary_source->begin.column << ": related location\n";
    }
}

CliDiagnostic make_cli_diagnostic(const Diagnostic &diagnostic,
                                  const std::vector<dsl::LoadedSource> &sources,
                                  const fs::path &canonical_root) {
    CliDiagnostic result;
    result.code = to_string(diagnostic.code);
    result.severity = to_string(diagnostic.severity);
    result.message = diagnostic.message;
    result.source = SourceLocation{display_source(diagnostic.source.file, sources, canonical_root),
                                   diagnostic.source.begin.line, diagnostic.source.begin.column};
    if (diagnostic.secondary_source.has_value()) {
        result.related.push_back(
            {{display_source(diagnostic.secondary_source->file, sources, canonical_root),
              diagnostic.secondary_source->begin.line, diagnostic.secondary_source->begin.column},
             "related location"});
    }
    return result;
}

int emit_model_diagnostic(std::ostream &output, std::ostream &error, int exit_status,
                          const Diagnostic &diagnostic,
                          const std::vector<dsl::LoadedSource> &sources,
                          const fs::path &canonical_root, bool json) {
    if (!json) {
        print_diagnostic(error, diagnostic, sources, canonical_root);
        return error ? exit_status : kExitOutput;
    }
    const auto *model_command = find_command("model");
    const auto cli_diagnostic = make_cli_diagnostic(diagnostic, sources, canonical_root);
    print_cli_error_json(output, exit_status, "model", model_command, nullptr, cli_diagnostic);
    if (!output) {
        error << "ltfa: failed to write error output\n";
        return kExitOutput;
    }
    return exit_status;
}

std::optional<OutputFormat> model_output_format(const ParsedArguments &parsed) {
    const auto format = parsed.option_value("format").value_or("human");
    if (format == "human") {
        return OutputFormat::human;
    }
    if (format == "json") {
        return OutputFormat::json;
    }
    if (format == "dot") {
        return OutputFormat::dot;
    }
    return std::nullopt;
}

std::optional<ModelOptions> model_options(const CommandDescriptor &command,
                                          const ParsedArguments &parsed, UsageError &usage) {
    ModelOptions options;
    const auto model_root = parsed.option_value("model-root").value_or("");
    const auto input = parsed.option_value("input").value_or("");
    if (model_root.empty()) {
        usage = make_usage_error("invalid_option_value", "option --model-root must not be empty",
                                 std::string{model_root}, &command);
        return std::nullopt;
    }
    if (input.empty()) {
        usage = make_usage_error("invalid_option_value", "option --input must not be empty",
                                 std::string{input}, &command);
        return std::nullopt;
    }
    options.model_root = fs::u8path(model_root);
    options.input = fs::u8path(input);
    if (options.input.is_absolute() || options.input.has_root_name() ||
        options.input.has_root_directory()) {
        usage = make_usage_error("invalid_option_value",
                                 "option --input must be relative to --model-root",
                                 std::string{input}, &command);
        return std::nullopt;
    }
    options.format = *model_output_format(parsed);
    options.include_restricted = parsed.has_option("include-restricted");
    return options;
}

int run_help(const ParsedArguments &parsed, std::ostream &output, std::ostream &error, bool json) {
    const CommandDescriptor *target = nullptr;
    if (!parsed.operands.empty()) {
        target = find_command(parsed.operands.front());
        if (target == nullptr) {
            const auto token = parsed.operands.front();
            const auto suggestion = suggest_command(token);
            const auto usage =
                make_usage_error("unknown_command", "unknown command " + std::string{token},
                                 std::string{token}, nullptr, suggestion);
            auto help_usage = usage;
            help_usage.operation = "help";
            return emit_usage_error(output, error, help_usage, json);
        }
    }
    const auto format = parsed.option_value("format").value_or("text");
    if (format == "json") {
        print_help_json(output, target);
    } else if (target == nullptr) {
        print_root_help(output);
    } else {
        print_command_help(output, *target);
    }
    if (!output) {
        error << "ltfa: failed to write help output\n";
        return kExitOutput;
    }
    return kExitSuccess;
}

int run_model(const CommandDescriptor &command, const ParsedArguments &parsed, std::ostream &output,
              std::ostream &error, bool json) {
    UsageError usage;
    const auto options = model_options(command, parsed, usage);
    if (!options.has_value()) {
        return emit_usage_error(output, error, usage, json);
    }

    std::error_code filesystem_error;
    const auto canonical_root = fs::canonical(options->model_root, filesystem_error);
    const auto loaded =
        dsl::load_document_tree(options->model_root, options->input, kDefaultModelLimits);
    if (!loaded.ok()) {
        if (loaded.error.has_value()) {
            return emit_model_diagnostic(output, error, kExitInput, *loaded.error, loaded.sources,
                                         filesystem_error ? fs::path{} : canonical_root, json);
        }
        return kExitInput;
    }

    const auto compiled = compile_model(*loaded.document, kDefaultModelLimits);
    if (!compiled.ok()) {
        if (compiled.error.has_value()) {
            return emit_model_diagnostic(output, error, kExitValidation, *compiled.error,
                                         loaded.sources, loaded.document->canonical_model_root,
                                         json);
        }
        return kExitValidation;
    }

    ModelOutputContext context;
    context.model_inputs.reserve(loaded.sources.size());
    for (const auto &source : loaded.sources) {
        context.model_inputs.push_back(
            display_source(source.id, loaded.sources, loaded.document->canonical_model_root));
    }
    if (options->include_restricted) {
        context.restricted_policy = RestrictedOutputPolicy::include_explicit;
    }

    switch (options->format) {
    case OutputFormat::human:
        output << render_model_human(*compiled.model, context);
        break;
    case OutputFormat::json:
        output << render_model_json(*compiled.model, context);
        break;
    case OutputFormat::dot:
        output << render_model_dot(*compiled.model, context);
        break;
    }
    if (!output) {
        error << "ltfa: failed to write model output\n";
        return kExitOutput;
    }
    return kExitSuccess;
}

} // namespace

int run_cli(int argc, char *argv[], std::ostream &output, std::ostream &error) {
    const bool json = json_requested(argc, argv);
    if (argc == 1) {
        const auto usage = make_usage_error("missing_command", "expected a command", "", nullptr);
        return emit_usage_error(output, error, usage, false);
    }
    if (argc == 2) {
        const std::string_view argument{argv[1]};
        if (argument == "--help" || argument == "-h") {
            print_root_help(output);
            return finish_output(output, error, "help");
        }
        if (argument == "--version") {
            output << project_name() << ' ' << version_string() << '\n';
            return finish_output(output, error, "version");
        }
    }

    const std::string_view command_name{argv[1]};
    const auto *command = find_command(command_name);
    if (command == nullptr) {
        const auto usage =
            make_usage_error("unknown_command", "unknown command " + std::string{command_name},
                             std::string{command_name}, nullptr, suggest_command(command_name));
        return emit_usage_error(output, error, usage, json);
    }

    ParsedArguments parsed;
    UsageError usage;
    if (!parse_command_arguments(*command, argc, argv, 2, parsed, usage)) {
        return emit_usage_error(output, error, usage, json);
    }
    if (parsed.has_option("help")) {
        print_command_help(output, *command);
        return finish_output(output, error, "help");
    }
    switch (command->handler) {
    case CommandHandler::help:
        return run_help(parsed, output, error, json);
    case CommandHandler::model:
        return run_model(*command, parsed, output, error, json);
    }
    return kExitUsage;
}

} // namespace ltfa::app
