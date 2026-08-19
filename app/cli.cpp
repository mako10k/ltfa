#include "cli.hpp"

#include "advanced_output.hpp"
#include "assurance_output.hpp"
#include "cli_contract.hpp"

#include "ltfa/dsl/include_resolver.hpp"
#include "ltfa/error.hpp"
#include "ltfa/failure_propagation.hpp"
#include "ltfa/model_compiler.hpp"
#include "ltfa/model_output.hpp"
#include "ltfa/path_analysis.hpp"
#include "ltfa/spof_analysis.hpp"
#include "ltfa/version.hpp"

#include <array>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <optional>
#include <ostream>
#include <sstream>
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
    DotView dot_view{DotView::raw};
    bool include_restricted{false};
};

struct AnalyzeOptions {
    fs::path model_root;
    fs::path input;
    std::string scope;
    std::string source;
    std::string target;
    std::optional<std::string> failure;
    OutputFormat format{OutputFormat::human};
    bool include_restricted{false};
    AssuranceBudgets budgets;
};

struct CutsOptions {
    ModelOptions model;
    std::string scope;
    std::string source;
    std::string target;
    ServiceCutBudget budget;
    std::size_t max_cut_size{2};
};

struct DiagnoseOptions {
    ModelOptions model;
    fs::path observations;
    std::string analysis_at;
    DiagnosisBudget diagnosis_budget;
    ObservationRecommendationBudget recommendation_budget;
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

[[nodiscard]] std::size_t remaining_work_budget(std::size_t limit, std::size_t used) noexcept {
    return used >= limit ? 0 : limit - used;
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
                          const fs::path &canonical_root, const CommandDescriptor &command,
                          bool json) {
    if (!json) {
        print_diagnostic(error, diagnostic, sources, canonical_root);
        return error ? exit_status : kExitOutput;
    }
    const auto cli_diagnostic = make_cli_diagnostic(diagnostic, sources, canonical_root);
    print_cli_error_json(output, exit_status, command.operation, &command, nullptr, cli_diagnostic);
    if (!output) {
        error << "ltfa: failed to write error output\n";
        return kExitOutput;
    }
    return exit_status;
}

int emit_analysis_diagnostic(std::ostream &output, std::ostream &error,
                             const CommandDescriptor &command, std::string message, bool json) {
    if (!json) {
        error << "LTANL-001 error: " << message << '\n'
              << "  help: ltfa help " << command.name << '\n';
        return error ? kExitValidation : kExitOutput;
    }
    CliDiagnostic diagnostic;
    diagnostic.code = "LTANL-001";
    diagnostic.severity = "error";
    diagnostic.message = std::move(message);
    print_cli_error_json(output, kExitValidation, command.operation, &command, nullptr, diagnostic);
    if (!output) {
        error << "ltfa: failed to write error output\n";
        return kExitOutput;
    }
    return kExitValidation;
}

int emit_operation_diagnostic(std::ostream &output, std::ostream &error,
                              const CommandDescriptor &command, int exit_status, std::string code,
                              std::string message, bool json) {
    if (!json) {
        error << code << " error: " << message << '\n'
              << "  help: ltfa help " << command.name << '\n';
        return error ? exit_status : kExitOutput;
    }
    CliDiagnostic diagnostic;
    diagnostic.code = std::move(code);
    diagnostic.severity = "error";
    diagnostic.message = std::move(message);
    print_cli_error_json(output, exit_status, command.operation, &command, nullptr, diagnostic);
    return output ? exit_status : kExitOutput;
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
    const auto dot_view = parse_dot_view(parsed.option_value("dot-view").value_or("raw"));
    if (!dot_view.has_value()) {
        usage =
            make_usage_error("invalid_option_value", "option --dot-view has an invalid value",
                             std::string{parsed.option_value("dot-view").value_or("")}, &command);
        return std::nullopt;
    }
    if (parsed.has_option("dot-view") && options.format != OutputFormat::dot) {
        usage = make_usage_error("option_dependency", "option --dot-view requires --format dot",
                                 "--dot-view", &command);
        return std::nullopt;
    }
    options.dot_view = *dot_view;
    options.include_restricted = parsed.has_option("include-restricted");
    return options;
}

bool parse_positive_count(const CommandDescriptor &command, const ParsedArguments &parsed,
                          std::string_view name, std::size_t default_value, std::size_t &result,
                          UsageError &usage) {
    const auto value = parsed.option_value(name);
    if (!value.has_value()) {
        result = default_value;
        return true;
    }
    std::size_t parsed_value = 0;
    const auto converted =
        std::from_chars(value->data(), value->data() + value->size(), parsed_value);
    if (value->empty() || converted.ec != std::errc{} ||
        converted.ptr != value->data() + value->size() || parsed_value == 0) {
        usage = make_usage_error("invalid_option_value",
                                 "option --" + std::string{name} +
                                     " must be a positive base-10 integer",
                                 std::string{*value}, &command);
        return false;
    }
    result = parsed_value;
    return true;
}

std::optional<CutsOptions> cuts_options(const CommandDescriptor &command,
                                        const ParsedArguments &parsed, UsageError &usage) {
    const auto common = model_options(command, parsed, usage);
    if (!common.has_value()) {
        return std::nullopt;
    }
    CutsOptions options;
    options.model = *common;
    options.scope = std::string{parsed.option_value("scope").value_or("")};
    options.source = std::string{parsed.option_value("source").value_or("")};
    options.target = std::string{parsed.option_value("target").value_or("")};
    if (!parse_positive_count(command, parsed, "max-cut-size", 2, options.max_cut_size, usage) ||
        !parse_positive_count(command, parsed, "max-candidate-sets",
                              options.budget.max_candidate_sets, options.budget.max_candidate_sets,
                              usage) ||
        !parse_positive_count(command, parsed, "max-results", options.budget.max_results,
                              options.budget.max_results, usage) ||
        !parse_positive_count(command, parsed, "max-visited-objects",
                              options.budget.max_total_visited_objects,
                              options.budget.max_total_visited_objects, usage) ||
        !parse_positive_count(command, parsed, "max-examined-relations",
                              options.budget.max_total_examined_relations,
                              options.budget.max_total_examined_relations, usage) ||
        !parse_positive_count(command, parsed, "max-flow-transitions",
                              options.budget.max_flow_transitions,
                              options.budget.max_flow_transitions, usage)) {
        return std::nullopt;
    }
    if (options.max_cut_size > 2) {
        usage = make_usage_error("invalid_option_value", "option --max-cut-size must be 1 or 2",
                                 std::to_string(options.max_cut_size), &command);
        return std::nullopt;
    }
    return options;
}

std::optional<DiagnoseOptions> diagnose_options(const CommandDescriptor &command,
                                                const ParsedArguments &parsed, UsageError &usage) {
    const auto common = model_options(command, parsed, usage);
    if (!common.has_value()) {
        return std::nullopt;
    }
    DiagnoseOptions options;
    options.model = *common;
    const auto observations = parsed.option_value("observations").value_or("");
    if (observations.empty()) {
        usage = make_usage_error("invalid_option_value", "option --observations must not be empty",
                                 "", &command);
        return std::nullopt;
    }
    options.observations = fs::u8path(observations);
    if (options.observations.is_absolute() || options.observations.has_root_name() ||
        options.observations.has_root_directory()) {
        usage = make_usage_error("invalid_option_value",
                                 "option --observations must be relative to --model-root",
                                 std::string{observations}, &command);
        return std::nullopt;
    }
    options.analysis_at = std::string{parsed.option_value("analysis-at").value_or("")};
    if (!parse_positive_count(command, parsed, "max-causal-objects",
                              options.diagnosis_budget.max_causal_objects,
                              options.diagnosis_budget.max_causal_objects, usage) ||
        !parse_positive_count(command, parsed, "max-hypotheses",
                              options.diagnosis_budget.max_hypotheses,
                              options.diagnosis_budget.max_hypotheses, usage) ||
        !parse_positive_count(command, parsed, "max-simulations",
                              options.diagnosis_budget.max_simulations,
                              options.diagnosis_budget.max_simulations, usage) ||
        !parse_positive_count(command, parsed, "max-evidence-comparisons",
                              options.diagnosis_budget.max_evidence_comparisons,
                              options.diagnosis_budget.max_evidence_comparisons, usage) ||
        !parse_positive_count(command, parsed, "max-candidate-probes",
                              options.recommendation_budget.max_candidate_probes,
                              options.recommendation_budget.max_candidate_probes, usage) ||
        !parse_positive_count(command, parsed, "max-recommendation-comparisons",
                              options.recommendation_budget.max_hypothesis_comparisons,
                              options.recommendation_budget.max_hypothesis_comparisons, usage) ||
        !parse_positive_count(command, parsed, "max-recommendations",
                              options.recommendation_budget.max_recommendations,
                              options.recommendation_budget.max_recommendations, usage)) {
        return std::nullopt;
    }
    return options;
}

std::optional<AnalyzeOptions> analyze_options(const CommandDescriptor &command,
                                              const ParsedArguments &parsed, UsageError &usage) {
    AnalyzeOptions options;
    const auto model_root = parsed.option_value("model-root").value_or("");
    const auto input = parsed.option_value("input").value_or("");
    const auto scope = parsed.option_value("scope").value_or("");
    const auto source = parsed.option_value("source").value_or("");
    const auto target = parsed.option_value("target").value_or("");
    const std::array<std::pair<std::string_view, std::string_view>, 5> required{{
        {"model-root", model_root},
        {"input", input},
        {"scope", scope},
        {"source", source},
        {"target", target},
    }};
    for (const auto &[name, value] : required) {
        if (value.empty()) {
            usage = make_usage_error("invalid_option_value",
                                     "option --" + std::string{name} + " must not be empty",
                                     std::string{value}, &command);
            return std::nullopt;
        }
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
    options.scope = scope;
    options.source = source;
    options.target = target;
    if (const auto failure = parsed.option_value("failure"); failure.has_value()) {
        if (failure->empty()) {
            usage = make_usage_error("invalid_option_value", "option --failure must not be empty",
                                     "", &command);
            return std::nullopt;
        }
        options.failure = std::string{*failure};
    }
    const auto format = parsed.option_value("format").value_or("human");
    options.format = format == "json" ? OutputFormat::json : OutputFormat::human;
    options.include_restricted = parsed.has_option("include-restricted");
    if (!parse_positive_count(command, parsed, "max-candidates", 1'024,
                              options.budgets.spof.max_candidates, usage) ||
        !parse_positive_count(command, parsed, "max-visited-objects", 2'000'000,
                              options.budgets.spof.max_total_visited_objects, usage) ||
        !parse_positive_count(command, parsed, "max-examined-relations", 10'000'000,
                              options.budgets.spof.max_total_examined_relations, usage) ||
        !parse_positive_count(command, parsed, "max-events", 1'000'000,
                              options.budgets.propagation.max_events, usage) ||
        !parse_positive_count(command, parsed, "max-state-transitions", 300'000,
                              options.budgets.propagation.max_state_transitions, usage)) {
        return std::nullopt;
    }
    options.budgets.path.max_visited_objects = options.budgets.spof.max_total_visited_objects;
    options.budgets.path.max_examined_relations = options.budgets.spof.max_total_examined_relations;
    return options;
}

std::optional<ScopeId> find_scope(const NormalizedModel &model, std::string_view external_id) {
    for (const auto &scope : model.scopes) {
        if (model.string(scope.external_id) == external_id) {
            return scope.id;
        }
    }
    return std::nullopt;
}

std::optional<ObjectRef> find_object(const NormalizedModel &model, std::string_view external_id) {
    for (const auto &entity : model.entities) {
        if (model.string(entity.external_id) == external_id) {
            return ObjectRef::from(entity.id);
        }
    }
    for (const auto &endpoint : model.endpoints) {
        if (model.string(endpoint.external_id) == external_id) {
            return ObjectRef::from(endpoint.id);
        }
    }
    return std::nullopt;
}

std::optional<FailureModeId> find_failure(const NormalizedModel &model,
                                          std::string_view external_id) {
    for (const auto &failure : model.failure_modes) {
        if (model.string(failure.external_id) == external_id) {
            return failure.id;
        }
    }
    return std::nullopt;
}

ScopeId object_scope(const NormalizedModel &model, ObjectRef object) {
    return object.kind == ObjectKind::entity ? model.entities[object.index].scope
                                             : model.endpoints[object.index].scope;
}

bool scope_contains_restricted(const NormalizedModel &model, ScopeId scope) {
    for (const auto &entity : model.entities) {
        if (entity.scope == scope && entity.sensitivity == Sensitivity::restricted) {
            return true;
        }
    }
    for (const auto &endpoint : model.endpoints) {
        if (endpoint.scope == scope && endpoint.sensitivity == Sensitivity::restricted) {
            return true;
        }
    }
    return false;
}

bool model_contains_restricted(const NormalizedModel &model) {
    for (const auto &entity : model.entities) {
        if (entity.sensitivity == Sensitivity::restricted) {
            return true;
        }
    }
    for (const auto &endpoint : model.endpoints) {
        if (endpoint.sensitivity == Sensitivity::restricted) {
            return true;
        }
    }
    return false;
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
                                         filesystem_error ? fs::path{} : canonical_root, command,
                                         json);
        }
        return kExitInput;
    }

    const auto compiled = compile_model(*loaded.document, kDefaultModelLimits);
    if (!compiled.ok()) {
        if (compiled.error.has_value()) {
            return emit_model_diagnostic(output, error, kExitValidation, *compiled.error,
                                         loaded.sources, loaded.document->canonical_model_root,
                                         command, json);
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
        DotOutputOptions dot_options;
        dot_options.view = options->dot_view;
        output << render_model_dot(*compiled.model, context, dot_options);
        break;
    }
    if (!output) {
        error << "ltfa: failed to write model output\n";
        return kExitOutput;
    }
    return kExitSuccess;
}

int run_analyze(const CommandDescriptor &command, const ParsedArguments &parsed,
                std::ostream &output, std::ostream &error, bool json) {
    UsageError usage;
    const auto options = analyze_options(command, parsed, usage);
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
                                         filesystem_error ? fs::path{} : canonical_root, command,
                                         json);
        }
        return kExitInput;
    }
    const auto compiled = compile_model(*loaded.document, kDefaultModelLimits);
    if (!compiled.ok()) {
        if (compiled.error.has_value()) {
            return emit_model_diagnostic(output, error, kExitValidation, *compiled.error,
                                         loaded.sources, loaded.document->canonical_model_root,
                                         command, json);
        }
        return kExitValidation;
    }
    const auto &model = *compiled.model;
    const auto scope = find_scope(model, options->scope);
    if (!scope.has_value()) {
        return emit_analysis_diagnostic(output, error, command,
                                        "scope not found: " + options->scope, json);
    }
    const auto source = find_object(model, options->source);
    if (!source.has_value()) {
        return emit_analysis_diagnostic(output, error, command,
                                        "source object not found: " + options->source, json);
    }
    const auto target = find_object(model, options->target);
    if (!target.has_value()) {
        return emit_analysis_diagnostic(output, error, command,
                                        "target object not found: " + options->target, json);
    }
    if (*source == *target) {
        return emit_analysis_diagnostic(output, error, command,
                                        "source and target must be distinct", json);
    }
    if (object_scope(model, *source) != *scope || object_scope(model, *target) != *scope) {
        return emit_analysis_diagnostic(
            output, error, command, "source and target must both belong to scope " + options->scope,
            json);
    }
    if (!options->include_restricted &&
        (scope_contains_restricted(model, *scope) ||
         (options->failure.has_value() && model_contains_restricted(model)))) {
        return emit_analysis_diagnostic(
            output, error, command,
            "restricted model IDs require --include-restricted for this analysis", json);
    }

    std::optional<FailureModeId> failure;
    if (options->failure.has_value()) {
        failure = find_failure(model, *options->failure);
        if (!failure.has_value()) {
            return emit_analysis_diagnostic(
                output, error, command, "declared failure not found: " + *options->failure, json);
        }
    }

    const auto snapshot = make_availability_snapshot(model);
    GraphFilter view;
    view.scope = *scope;
    view.relation_kinds = {
        RelationKind::network,       RelationKind::power,          RelationKind::serial,
        RelationKind::contact,       RelationKind::analog_current, RelationKind::carried_by,
        RelationKind::uses_underlay, RelationKind::terminates_on,  RelationKind::routes_via,
        RelationKind::depends_on,    RelationKind::powered_by,
    };
    view.direction = TraversalDirection::undirected;
    AssuranceReport report;
    report.scope = *scope;
    report.source = *source;
    report.target = *target;
    report.include_restricted = options->include_restricted;
    report.budgets = options->budgets;
    for (const auto &loaded_source : loaded.sources) {
        report.model_inputs.push_back(display_source(loaded_source.id, loaded.sources,
                                                     loaded.document->canonical_model_root));
    }

    report.path = find_path(model, snapshot, *source, *target, view, options->budgets.path);
    if (report.path.status == AnalysisStatus::invalid) {
        return emit_analysis_diagnostic(
            output, error, command, "path analysis rejected the selected normalized view", json);
    }
    report.incomplete = report.path.status == AnalysisStatus::incomplete;
    if (report.path.status == AnalysisStatus::complete && report.path.reachable) {
        SpofRequest spof_request;
        spof_request.source = *source;
        spof_request.targets.push_back(*target);
        spof_request.view = view;
        auto spof_budget = options->budgets.spof;
        spof_budget.max_total_visited_objects = remaining_work_budget(
            spof_budget.max_total_visited_objects, report.path.usage.visited_objects);
        spof_budget.max_total_examined_relations = remaining_work_budget(
            spof_budget.max_total_examined_relations, report.path.usage.examined_relations);
        report.spof = analyze_spof(model, snapshot, spof_request, spof_budget);
        if (report.spof->status == AnalysisStatus::invalid) {
            return emit_analysis_diagnostic(output, error, command,
                                            "SPOF analysis rejected the selected normalized view",
                                            json);
        }
        report.incomplete = report.incomplete || report.spof->status == AnalysisStatus::incomplete;
    }

    if (failure.has_value()) {
        report.failure_external_id = *options->failure;
        report.propagation =
            propagate_failure(model, snapshot, FailureInjection::for_declared_mode(*failure),
                              options->budgets.propagation);
        if (report.propagation->status == PropagationStatus::invalid) {
            return emit_analysis_diagnostic(
                output, error, command,
                "failure propagation rejected the selected normalized model", json);
        }
        report.incomplete =
            report.incomplete || report.propagation->status == PropagationStatus::incomplete;
    }

    if (options->format == OutputFormat::json) {
        output << render_assurance_json(model, report);
    } else {
        output << render_assurance_human(model, report);
    }
    if (!output) {
        error << "ltfa: failed to write assurance output\n";
        return kExitOutput;
    }
    return kExitSuccess;
}

int run_cuts(const CommandDescriptor &command, const ParsedArguments &parsed, std::ostream &output,
             std::ostream &error, bool json) {
    UsageError usage;
    const auto options = cuts_options(command, parsed, usage);
    if (!options.has_value()) {
        return emit_usage_error(output, error, usage, json);
    }
    std::error_code filesystem_error;
    const auto canonical_root = fs::canonical(options->model.model_root, filesystem_error);
    const auto loaded = dsl::load_document_tree(options->model.model_root, options->model.input,
                                                kDefaultModelLimits);
    if (!loaded.ok()) {
        return loaded.error.has_value()
                   ? emit_model_diagnostic(output, error, kExitInput, *loaded.error, loaded.sources,
                                           filesystem_error ? fs::path{} : canonical_root, command,
                                           json)
                   : kExitInput;
    }
    const auto compiled = compile_model(*loaded.document, kDefaultModelLimits);
    if (!compiled.ok()) {
        return compiled.error.has_value()
                   ? emit_model_diagnostic(output, error, kExitValidation, *compiled.error,
                                           loaded.sources, loaded.document->canonical_model_root,
                                           command, json)
                   : kExitValidation;
    }
    const auto &model = *compiled.model;
    const auto scope = find_scope(model, options->scope);
    const auto source = find_object(model, options->source);
    const auto target = find_object(model, options->target);
    if (!scope.has_value()) {
        return emit_analysis_diagnostic(output, error, command,
                                        "scope not found: " + options->scope, json);
    }
    if (!source.has_value() || !target.has_value()) {
        return emit_analysis_diagnostic(output, error, command,
                                        "source or target object was not found", json);
    }
    if (*source == *target || object_scope(model, *source) != *scope ||
        object_scope(model, *target) != *scope) {
        return emit_analysis_diagnostic(
            output, error, command,
            "source and target must be distinct objects in scope " + options->scope, json);
    }
    if (!options->model.include_restricted && scope_contains_restricted(model, *scope)) {
        return emit_analysis_diagnostic(
            output, error, command,
            "restricted model IDs require --include-restricted for this analysis", json);
    }
    GraphFilter view;
    view.scope = *scope;
    view.relation_kinds = {
        RelationKind::network,       RelationKind::power,          RelationKind::serial,
        RelationKind::contact,       RelationKind::analog_current, RelationKind::carried_by,
        RelationKind::uses_underlay, RelationKind::terminates_on,  RelationKind::routes_via,
        RelationKind::depends_on,    RelationKind::powered_by,
    };
    view.direction = TraversalDirection::undirected;
    ServiceCutRequest request;
    request.source = *source;
    request.target = *target;
    request.view = view;
    request.max_cut_size = options->max_cut_size;
    const auto result =
        analyze_service_cuts(model, make_availability_snapshot(model), request, options->budget);
    if (result.status == AnalysisStatus::invalid) {
        return emit_analysis_diagnostic(output, error, command,
                                        "service-cut analysis rejected the selected view", json);
    }
    output << (options->model.format == OutputFormat::json
                   ? render_service_cuts_json(model, result)
                   : render_service_cuts_human(model, result));
    return finish_output(output, error, "service-cut");
}

std::optional<std::string> read_observation_file(const fs::path &root, const fs::path &relative,
                                                 std::string &message) {
    std::error_code error;
    const auto canonical_root = fs::canonical(root, error);
    if (error) {
        message = "model root cannot be resolved";
        return std::nullopt;
    }
    const auto canonical_input = fs::canonical(canonical_root / relative, error);
    if (error) {
        message = "observation input cannot be resolved";
        return std::nullopt;
    }
    const auto within = canonical_input.lexically_relative(canonical_root);
    if (within.empty() || within.is_absolute() || *within.begin() == "..") {
        message = "observation input must remain below --model-root";
        return std::nullopt;
    }
    const auto size = fs::file_size(canonical_input, error);
    if (error || size > kDefaultModelLimits.max_observation_input_bytes) {
        message = "observation input exceeds the configured byte limit or is not a file";
        return std::nullopt;
    }
    std::ifstream input{canonical_input, std::ios::binary};
    if (!input) {
        message = "observation input cannot be opened";
        return std::nullopt;
    }
    std::ostringstream content;
    content << input.rdbuf();
    if (!input.good() && !input.eof()) {
        message = "observation input cannot be read completely";
        return std::nullopt;
    }
    return content.str();
}

int run_diagnose(const CommandDescriptor &command, const ParsedArguments &parsed,
                 std::ostream &output, std::ostream &error, bool json) {
    UsageError usage;
    const auto options = diagnose_options(command, parsed, usage);
    if (!options.has_value()) {
        return emit_usage_error(output, error, usage, json);
    }
    std::error_code filesystem_error;
    const auto canonical_root = fs::canonical(options->model.model_root, filesystem_error);
    const auto loaded = dsl::load_document_tree(options->model.model_root, options->model.input,
                                                kDefaultModelLimits);
    if (!loaded.ok()) {
        return loaded.error.has_value()
                   ? emit_model_diagnostic(output, error, kExitInput, *loaded.error, loaded.sources,
                                           filesystem_error ? fs::path{} : canonical_root, command,
                                           json)
                   : kExitInput;
    }
    const auto compiled = compile_model(*loaded.document, kDefaultModelLimits);
    if (!compiled.ok()) {
        return compiled.error.has_value()
                   ? emit_model_diagnostic(output, error, kExitValidation, *compiled.error,
                                           loaded.sources, loaded.document->canonical_model_root,
                                           command, json)
                   : kExitValidation;
    }
    const auto &model = *compiled.model;
    if (!options->model.include_restricted && model_contains_restricted(model)) {
        return emit_analysis_diagnostic(
            output, error, command,
            "restricted model IDs require --include-restricted for diagnosis", json);
    }
    std::string input_error;
    const auto observation_text =
        read_observation_file(options->model.model_root, options->observations, input_error);
    if (!observation_text.has_value()) {
        return emit_operation_diagnostic(output, error, command, kExitInput, "LTOBS-001",
                                         std::move(input_error), json);
    }
    const auto reference_time = parse_observation_timestamp(options->analysis_at);
    if (!reference_time.ok()) {
        return emit_operation_diagnostic(output, error, command, kExitValidation, "LTOBS-002",
                                         "--analysis-at must be a valid RFC3339 timestamp", json);
    }
    const auto decoded = decode_observation_json_lines(
        *observation_text, StringId{static_cast<std::uint32_t>(model.strings.size())},
        kDefaultModelLimits);
    if (!decoded.ok()) {
        return emit_operation_diagnostic(output, error, command, kExitInput, "LTOBS-003",
                                         decoded.error.has_value() ? decoded.error->message
                                                                   : "observation decoding failed",
                                         json);
    }
    auto indexed = build_evidence_index(model, decoded.observations, *reference_time.timestamp);
    if (!indexed.ok()) {
        return emit_operation_diagnostic(
            output, error, command, kExitValidation, "LTOBS-004",
            indexed.error.has_value() ? indexed.error->message : "evidence indexing failed", json);
    }
    const auto diagnosis = diagnose_observations(model, *indexed.index, options->diagnosis_budget);
    if (diagnosis.status == DiagnosisStatus::invalid) {
        return emit_analysis_diagnostic(output, error, command,
                                        "diagnosis rejected the normalized evidence", json);
    }
    const auto recommendations = recommend_next_observations(model, *indexed.index, diagnosis,
                                                             options->recommendation_budget);
    if (recommendations.status == RecommendationStatus::invalid) {
        return emit_analysis_diagnostic(output, error, command,
                                        "recommendation rejected the diagnosis", json);
    }
    DiagnosisOutputContext context;
    for (const auto &source : loaded.sources) {
        context.model_inputs.push_back(
            display_source(source.id, loaded.sources, loaded.document->canonical_model_root));
    }
    context.observation_inputs.push_back(options->observations.generic_string());
    if (options->model.include_restricted) {
        context.restricted_policy = RestrictedOutputPolicy::include_explicit;
    }
    switch (options->model.format) {
    case OutputFormat::human:
        output << render_diagnosis_cli_human(model, diagnosis, recommendations);
        break;
    case OutputFormat::json:
        output << render_diagnosis_cli_json(model, *indexed.index, diagnosis, recommendations,
                                            context);
        break;
    case OutputFormat::dot:
        output << render_diagnosis_dot(model, *indexed.index, diagnosis, context);
        break;
    }
    return finish_output(output, error, "diagnosis");
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
    case CommandHandler::analyze:
        return run_analyze(*command, parsed, output, error, json);
    case CommandHandler::cuts:
        return run_cuts(*command, parsed, output, error, json);
    case CommandHandler::diagnose:
        return run_diagnose(*command, parsed, output, error, json);
    }
    return kExitUsage;
}

} // namespace ltfa::app
#include "ltfa/cut_analysis.hpp"
#include "ltfa/diagnosis.hpp"
#include "ltfa/diagnosis_output.hpp"
#include "ltfa/observation.hpp"
#include "ltfa/observation_recommendation.hpp"
