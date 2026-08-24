#pragma once

#include <cstddef>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ltfa::app {

enum class OptionKind {
    flag,
    value,
};

enum class CommandHandler {
    help,
    guide,
    model,
    analyze,
    cuts,
    diagnose,
};

struct OperandDescriptor {
    std::string_view name;
    std::string_view value_type;
    bool required;
    std::size_t position;
};

struct OptionDescriptor {
    std::string_view name;
    std::string_view short_name;
    OptionKind kind;
    std::string_view value_name;
    std::string_view value_type;
    bool required;
    bool repeatable;
    std::string_view default_value;
    std::string_view enum_values;
    std::string_view conflicts;
    std::string_view required_options;
    std::string_view description;
};

struct ExitStatusDescriptor {
    int code;
    std::string_view meaning;
};

struct ExampleDescriptor {
    std::string_view id;
    std::string_view invocation;
    std::string_view summary;
};

struct CommandDescriptor {
    std::string_view name;
    std::string_view operation;
    std::string_view summary;
    std::string_view usage;
    CommandHandler handler;
    const OperandDescriptor *operands;
    std::size_t operand_count;
    const OptionDescriptor *options;
    std::size_t option_count;
    std::string_view input;
    std::string_view output_formats;
    std::string_view output_payload;
    std::string_view effect;
    std::string_view result_schemas;
    const ExitStatusDescriptor *exit_statuses;
    std::size_t exit_status_count;
    const ExampleDescriptor *examples;
    std::size_t example_count;
};

struct CommandCatalog {
    const CommandDescriptor *commands;
    std::size_t command_count;
};

struct UsageError {
    std::string kind;
    std::string message;
    std::string operation;
    std::string token;
    std::string suggestion;
    std::string guide_target;
    const CommandDescriptor *help_target{nullptr};
};

struct SourceLocation {
    std::string file;
    std::size_t line;
    std::size_t column;
};

struct RelatedLocation {
    SourceLocation source;
    std::string message;
};

struct CliDiagnostic {
    std::string code;
    std::string severity;
    std::string message;
    std::optional<SourceLocation> source;
    std::vector<RelatedLocation> related;
};

[[nodiscard]] CommandCatalog command_catalog();
[[nodiscard]] int cli_contract_version() noexcept;
[[nodiscard]] const CommandDescriptor *find_command(std::string_view name);
[[nodiscard]] const OptionDescriptor *find_option(const CommandDescriptor &command,
                                                  std::string_view name, bool short_name = false);
[[nodiscard]] std::string suggest_command(std::string_view name);
[[nodiscard]] std::string suggest_option(const CommandDescriptor &command, std::string_view name);
[[nodiscard]] bool enum_contains(std::string_view values, std::string_view value);
[[nodiscard]] std::string render_enum_values(std::string_view values);
[[nodiscard]] bool json_requested(int argc, char *argv[]);

void print_root_help(std::ostream &output);
void print_command_help(std::ostream &output, const CommandDescriptor &command);
void print_help_json(std::ostream &output, const CommandDescriptor *command);
void print_usage_error(std::ostream &error, const UsageError &usage);
void print_error_navigation(std::ostream &error, const CommandDescriptor *help_target,
                            const UsageError *usage, const CliDiagnostic &diagnostic);
void print_cli_error_json(std::ostream &output, int exit_status, std::string_view operation,
                          const CommandDescriptor *help_target, const UsageError *usage,
                          const CliDiagnostic &diagnostic);
void print_bash_completion_query(std::ostream &output, int argc, char *argv[], int begin);

} // namespace ltfa::app
