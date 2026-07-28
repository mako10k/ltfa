#include "ltfa/dsl/include_resolver.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ltfa::dsl {
namespace {

namespace fs = std::filesystem;

struct ActiveSource {
    fs::path canonical_path;
    SourceSpan header_source;
};

[[nodiscard]] Diagnostic make_error(DiagnosticCode code, SourceSpan source, std::string message) {
    return {
        code, DiagnosticSeverity::error, source, std::nullopt, std::nullopt, std::move(message),
    };
}

[[nodiscard]] SourceSpan synthetic_source(StringId file = StringId{0}) noexcept {
    return {file, {0, 1, 1}, {0, 1, 1}};
}

[[nodiscard]] bool is_ascii_alpha(char value) noexcept {
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
}

[[nodiscard]] bool is_ascii_digit(char value) noexcept { return value >= '0' && value <= '9'; }

[[nodiscard]] bool has_uri_scheme(std::string_view text) noexcept {
    if (text.empty() || !is_ascii_alpha(text.front())) {
        return false;
    }
    for (std::size_t index = 1; index < text.size(); ++index) {
        const char value = text[index];
        if (value == ':') {
            return true;
        }
        if (!is_ascii_alpha(value) && !is_ascii_digit(value) && value != '+' && value != '-' &&
            value != '.') {
            return false;
        }
    }
    return false;
}

[[nodiscard]] bool has_percent_environment_reference(std::string_view text) noexcept {
    for (std::size_t begin = 0; begin < text.size(); ++begin) {
        if (text[begin] != '%' || begin + 2 >= text.size()) {
            continue;
        }
        std::size_t end = begin + 1;
        if (!is_ascii_alpha(text[end]) && text[end] != '_') {
            continue;
        }
        ++end;
        while (end < text.size() &&
               (is_ascii_alpha(text[end]) || is_ascii_digit(text[end]) || text[end] == '_')) {
            ++end;
        }
        if (end < text.size() && text[end] == '%') {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool has_environment_expansion(std::string_view text) noexcept {
    return text.find('$') != std::string_view::npos || (!text.empty() && text.front() == '~') ||
           has_percent_environment_reference(text);
}

[[nodiscard]] bool path_is_within(const fs::path &root, const fs::path &candidate) noexcept {
    auto root_part = root.begin();
    auto candidate_part = candidate.begin();
    for (; root_part != root.end(); ++root_part, ++candidate_part) {
        if (candidate_part == candidate.end() || *root_part != *candidate_part) {
            return false;
        }
    }
    return true;
}

class Resolver {
  public:
    Resolver(fs::path canonical_root, const ModelLimits &limits)
        : canonical_root_(std::move(canonical_root)), limits_(limits) {}

    [[nodiscard]] LoadResult load(const fs::path &entry_file) {
        const bool loaded = load_file(entry_file, synthetic_source(), 0);
        if (!loaded && !error_.has_value()) {
            fail(DiagnosticCode::include_io, synthetic_source(), "failed to load model input");
        }
        if (loaded) {
            validate_scope_defaults();
        }
        if (error_.has_value()) {
            return {std::move(sources_), std::nullopt, std::move(error_)};
        }
        LoadedDocument document{
            canonical_root_,
            std::move(statements_),
            total_source_bytes_,
        };
        return {std::move(sources_), std::move(document), std::nullopt};
    }

  private:
    [[nodiscard]] bool load_file(const fs::path &candidate_path, SourceSpan origin,
                                 std::uint32_t depth) {
        std::error_code filesystem_error;
        const auto canonical_path = fs::canonical(candidate_path, filesystem_error);
        if (filesystem_error) {
            fail(DiagnosticCode::include_io, origin, "cannot resolve local model file");
            return false;
        }
        if (!path_is_within(canonical_root_, canonical_path)) {
            fail(DiagnosticCode::include_escape, origin,
                 "model file resolves outside the selected model root");
            return false;
        }
        if (!fs::is_regular_file(canonical_path, filesystem_error) || filesystem_error) {
            fail(DiagnosticCode::include_io, origin, "model input is not a regular local file");
            return false;
        }

        for (const auto &active : active_sources_) {
            if (active.canonical_path == canonical_path) {
                auto diagnostic =
                    make_error(DiagnosticCode::include_cycle, origin, "include cycle detected");
                diagnostic.secondary_source = active.header_source;
                error_ = std::move(diagnostic);
                return false;
            }
        }

        const auto source_id = source_id_for(canonical_path, origin);
        if (!source_id.has_value()) {
            return false;
        }

        const auto byte_size = fs::file_size(canonical_path, filesystem_error);
        if (filesystem_error) {
            fail(DiagnosticCode::include_io, origin, "cannot determine model file size");
            return false;
        }
        if (byte_size > limits_.max_source_file_bytes) {
            fail(DiagnosticCode::limit_source_file_bytes, origin,
                 "model file exceeds the configured per-file byte limit");
            return false;
        }
        if (byte_size > limits_.max_total_source_bytes - total_source_bytes_) {
            fail(DiagnosticCode::limit_total_source_bytes, origin,
                 "loaded model files exceed the configured total byte limit");
            return false;
        }
        if (byte_size > static_cast<std::uintmax_t>(std::numeric_limits<std::size_t>::max()) ||
            byte_size > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max())) {
            fail(DiagnosticCode::limit_source_file_bytes, origin,
                 "model file is too large for this build");
            return false;
        }

        auto contents = read_file(canonical_path, static_cast<std::size_t>(byte_size), origin);
        if (!contents.has_value()) {
            return false;
        }
        total_source_bytes_ += static_cast<std::uint64_t>(byte_size);
        sources_[source_id->value()].byte_size = static_cast<std::uint64_t>(byte_size);

        auto parsed = parse_document(*contents, *source_id, limits_);
        if (!parsed.ok()) {
            error_ = std::move(parsed.error);
            return false;
        }
        sources_[source_id->value()].header_source = parsed.document->header_source;
        active_sources_.push_back({canonical_path, parsed.document->header_source});

        for (auto &statement : parsed.document->statements) {
            const auto *include = std::get_if<IncludeStatement>(&statement);
            if (include == nullptr) {
                statements_.push_back(std::move(statement));
                continue;
            }
            if (!load_include(*include, canonical_path, depth)) {
                return false;
            }
        }
        active_sources_.pop_back();
        return true;
    }

    [[nodiscard]] bool load_include(const IncludeStatement &include, const fs::path &including_file,
                                    std::uint32_t depth) {
        if (depth >= limits_.max_include_depth) {
            fail(DiagnosticCode::include_depth, include.path.source,
                 "include depth exceeds the configured limit of " +
                     std::to_string(limits_.max_include_depth));
            return false;
        }

        const auto path_text = std::string_view{include.path.text};
        if (path_text.empty()) {
            fail(DiagnosticCode::include_invalid, include.path.source,
                 "include path must not be empty");
            return false;
        }
        if (has_uri_scheme(path_text)) {
            fail(DiagnosticCode::include_invalid, include.path.source,
                 "URI includes are not allowed");
            return false;
        }
        if (has_environment_expansion(path_text)) {
            fail(DiagnosticCode::include_invalid, include.path.source,
                 "environment expansion is not allowed in include paths");
            return false;
        }

        const auto relative_path = fs::u8path(include.path.text);
        if (relative_path.is_absolute() || relative_path.has_root_name() ||
            relative_path.has_root_directory()) {
            fail(DiagnosticCode::include_escape, include.path.source,
                 "include path must be relative to the including file");
            return false;
        }

        const auto candidate =
            (including_file.parent_path() / relative_path.lexically_normal()).lexically_normal();
        if (!path_is_within(canonical_root_, candidate)) {
            fail(DiagnosticCode::include_escape, include.path.source,
                 "include path escapes the selected model root");
            return false;
        }
        return load_file(candidate, include.path.source, depth + 1);
    }

    [[nodiscard]] std::optional<StringId> source_id_for(const fs::path &canonical_path,
                                                        SourceSpan origin) {
        for (const auto &source : sources_) {
            if (source.canonical_path == canonical_path) {
                return source.id;
            }
        }
        if (sources_.size() >= StringId::invalid_value()) {
            fail(DiagnosticCode::limit_exceeded, origin, "too many model source files");
            return std::nullopt;
        }
        const StringId id{static_cast<std::uint32_t>(sources_.size())};
        sources_.push_back({id, canonical_path, 0, std::nullopt});
        return id;
    }

    void validate_scope_defaults() {
        std::vector<std::string_view> scope_ids;
        for (const auto &statement : statements_) {
            const auto *scope = std::get_if<ScopeStatement>(&statement);
            if (scope == nullptr) {
                continue;
            }
            bool seen = false;
            for (const auto existing : scope_ids) {
                if (existing == scope->id.text) {
                    seen = true;
                    break;
                }
            }
            if (!seen) {
                scope_ids.push_back(scope->id.text);
            }
        }
        if (scope_ids.size() <= 1) {
            return;
        }

        for (const auto &statement : statements_) {
            const auto *entity = std::get_if<EntityStatement>(&statement);
            if (entity == nullptr) {
                continue;
            }
            bool has_scope = false;
            for (const auto &attribute : entity->attributes) {
                if (attribute.key == "scope") {
                    has_scope = true;
                    break;
                }
            }
            if (!has_scope) {
                fail(DiagnosticCode::dsl_ambiguous_scope, entity->id.source,
                     "entity must declare scope when the loaded model has multiple scopes");
                return;
            }
        }
    }

    [[nodiscard]] std::optional<std::string> read_file(const fs::path &canonical_path,
                                                       std::size_t byte_size, SourceSpan origin) {
        std::ifstream input{canonical_path, std::ios::binary};
        if (!input) {
            fail(DiagnosticCode::include_io, origin, "cannot open local model file");
            return std::nullopt;
        }

        std::string contents(byte_size, '\0');
        if (byte_size > 0) {
            input.read(contents.data(), static_cast<std::streamsize>(byte_size));
            if (input.gcount() != static_cast<std::streamsize>(byte_size)) {
                fail(DiagnosticCode::include_io, origin, "model file changed while being read");
                return std::nullopt;
            }
        }
        char extra = '\0';
        if (input.get(extra)) {
            fail(DiagnosticCode::include_io, origin, "model file changed while being read");
            return std::nullopt;
        }
        if (input.bad()) {
            fail(DiagnosticCode::include_io, origin, "failed while reading local model file");
            return std::nullopt;
        }
        return contents;
    }

    void fail(DiagnosticCode code, SourceSpan source, std::string message) {
        if (!error_.has_value()) {
            error_ = make_error(code, source, std::move(message));
        }
    }

    fs::path canonical_root_;
    const ModelLimits &limits_;
    std::vector<LoadedSource> sources_;
    std::vector<Statement> statements_;
    std::vector<ActiveSource> active_sources_;
    std::uint64_t total_source_bytes_{0};
    std::optional<Diagnostic> error_;
};

} // namespace

LoadResult load_document_tree(const fs::path &model_root, const fs::path &entry_file,
                              const ModelLimits &limits) {
    std::error_code filesystem_error;
    const auto canonical_root = fs::canonical(model_root, filesystem_error);
    if (filesystem_error || !fs::is_directory(canonical_root, filesystem_error) ||
        filesystem_error) {
        return {{},
                std::nullopt,
                make_error(DiagnosticCode::include_io, synthetic_source(),
                           "model root must be an existing local directory")};
    }

    const auto entry_candidate =
        entry_file.is_absolute() ? entry_file : canonical_root / entry_file.lexically_normal();
    Resolver resolver{canonical_root, limits};
    return resolver.load(entry_candidate);
}

} // namespace ltfa::dsl
