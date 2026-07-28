#include "cli.hpp"

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

struct ModelOptions {
    fs::path model_root;
    fs::path input;
    OutputFormat format{OutputFormat::human};
    bool include_restricted{false};
};

void print_usage(std::ostream &output) {
    output << "LTFA - lightweight multilayer topology and fault analyzer\n"
           << "\n"
           << "Usage:\n"
           << "  ltfa model --model-root <directory> --input <file> "
              "[--format human|json|dot] [--include-restricted]\n"
           << "  ltfa --help\n"
           << "  ltfa --version\n"
           << "\n"
           << "Commands:\n"
           << "  model  Load, validate, normalize, and render one local topology model.\n"
           << "\n"
           << "The current core is offline and does not execute probes or topology input.\n";
}

void print_model_usage(std::ostream &output) {
    output << "Usage:\n"
           << "  ltfa model --model-root <directory> --input <file> "
              "[--format human|json|dot] [--include-restricted]\n"
           << "\n"
           << "Options:\n"
           << "  --model-root <directory>  Explicit local root for the entry file and includes.\n"
           << "  --input <file>            Entry file relative to the model root.\n"
           << "  --format <format>         human (default), json, or dot.\n"
           << "  --include-restricted      Explicitly include restricted model objects.\n";
}

int usage_error(std::ostream &error, std::string_view message) {
    error << "ltfa: " << message << "; use 'ltfa model --help'\n";
    return kExitUsage;
}

std::optional<OutputFormat> parse_format(std::string_view value) {
    if (value == "human") {
        return OutputFormat::human;
    }
    if (value == "json") {
        return OutputFormat::json;
    }
    if (value == "dot") {
        return OutputFormat::dot;
    }
    return std::nullopt;
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

std::optional<ModelOptions> parse_model_options(int argc, char *argv[], std::ostream &error) {
    ModelOptions options;
    bool has_model_root = false;
    bool has_input = false;
    bool has_format = false;

    for (int index = 2; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--model-root" || argument == "--input" || argument == "--format") {
            if (index + 1 >= argc) {
                usage_error(error, std::string{argument} + " requires a value");
                return std::nullopt;
            }
            const std::string_view value{argv[++index]};
            if (argument == "--model-root") {
                if (has_model_root) {
                    usage_error(error, "--model-root may be specified only once");
                    return std::nullopt;
                }
                options.model_root = fs::u8path(value);
                has_model_root = true;
            } else if (argument == "--input") {
                if (has_input) {
                    usage_error(error, "--input may be specified only once");
                    return std::nullopt;
                }
                options.input = fs::u8path(value);
                has_input = true;
            } else {
                if (has_format) {
                    usage_error(error, "--format may be specified only once");
                    return std::nullopt;
                }
                const auto format = parse_format(value);
                if (!format.has_value()) {
                    usage_error(error, "--format must be human, json, or dot");
                    return std::nullopt;
                }
                options.format = *format;
                has_format = true;
            }
        } else if (argument == "--include-restricted") {
            if (options.include_restricted) {
                usage_error(error, "--include-restricted may be specified only once");
                return std::nullopt;
            }
            options.include_restricted = true;
        } else {
            usage_error(error, "unknown model option '" + std::string{argument} + "'");
            return std::nullopt;
        }
    }

    if (!has_model_root) {
        usage_error(error, "--model-root is required");
        return std::nullopt;
    }
    if (!has_input) {
        usage_error(error, "--input is required");
        return std::nullopt;
    }
    if (options.model_root.empty()) {
        usage_error(error, "--model-root must not be empty");
        return std::nullopt;
    }
    if (options.input.empty()) {
        usage_error(error, "--input must not be empty");
        return std::nullopt;
    }
    if (options.input.is_absolute() || options.input.has_root_name() ||
        options.input.has_root_directory()) {
        usage_error(error, "--input must be relative to --model-root");
        return std::nullopt;
    }
    return options;
}

int run_model(int argc, char *argv[], std::ostream &output, std::ostream &error) {
    if (argc == 3 && (std::string_view{argv[2]} == "--help" || std::string_view{argv[2]} == "-h")) {
        print_model_usage(output);
        return kExitSuccess;
    }

    const auto options = parse_model_options(argc, argv, error);
    if (!options.has_value()) {
        return kExitUsage;
    }

    std::error_code filesystem_error;
    const auto canonical_root = fs::canonical(options->model_root, filesystem_error);
    const auto loaded =
        dsl::load_document_tree(options->model_root, options->input, kDefaultModelLimits);
    if (!loaded.ok()) {
        if (loaded.error.has_value()) {
            print_diagnostic(error, *loaded.error, loaded.sources,
                             filesystem_error ? fs::path{} : canonical_root);
        }
        return kExitInput;
    }

    const auto compiled = compile_model(*loaded.document, kDefaultModelLimits);
    if (!compiled.ok()) {
        if (compiled.error.has_value()) {
            print_diagnostic(error, *compiled.error, loaded.sources,
                             loaded.document->canonical_model_root);
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
    if (argc == 1) {
        print_usage(output);
        return kExitSuccess;
    }
    if (argc == 2) {
        const std::string_view argument{argv[1]};
        if (argument == "--help" || argument == "-h") {
            print_usage(output);
            return kExitSuccess;
        }
        if (argument == "--version") {
            output << project_name() << ' ' << version_string() << '\n';
            return output ? kExitSuccess : kExitOutput;
        }
    }
    if (argc >= 2 && std::string_view{argv[1]} == "model") {
        return run_model(argc, argv, output, error);
    }

    error << "ltfa: unsupported arguments; use --help\n";
    return kExitUsage;
}

} // namespace ltfa::app
