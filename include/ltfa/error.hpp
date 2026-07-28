#pragma once

#include "ltfa/ids.hpp"
#include "ltfa/source.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace ltfa {

enum class DiagnosticSeverity {
    error,
    warning,
};

enum class DiagnosticCode {
    dsl_invalid_header,
    dsl_unexpected_token,
    dsl_invalid_utf8,
    dsl_bom_not_allowed,
    dsl_unterminated_string,
    dsl_invalid_character,
    dsl_unknown_statement,
    dsl_invalid_attribute,
    dsl_duplicate_attribute,
    dsl_invalid_value,
    dsl_missing_required_field,
    dsl_ambiguous_scope,
    include_invalid,
    include_escape,
    include_cycle,
    include_depth,
    include_io,
    id_invalid,
    id_duplicate,
    reference_unresolved,
    endpoint_incompatible,
    layer_incompatible,
    dependency_cycle,
    limit_exceeded,
    limit_source_file_bytes,
    limit_total_source_bytes,
    limit_line_bytes,
    observation_invalid,
};

[[nodiscard]] std::string_view to_string(DiagnosticSeverity value) noexcept;
[[nodiscard]] std::string_view to_string(DiagnosticCode value) noexcept;

struct Diagnostic {
    DiagnosticCode code{DiagnosticCode::dsl_unexpected_token};
    DiagnosticSeverity severity{DiagnosticSeverity::error};
    SourceSpan source;
    std::optional<SourceSpan> secondary_source;
    std::optional<StringId> related_external_id;
    std::string message;
};

} // namespace ltfa
