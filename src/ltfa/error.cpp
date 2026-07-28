#include "ltfa/error.hpp"

namespace ltfa {

std::string_view to_string(DiagnosticSeverity value) noexcept {
    switch (value) {
    case DiagnosticSeverity::error:
        return "error";
    case DiagnosticSeverity::warning:
        return "warning";
    }
    return "unknown";
}

std::string_view to_string(DiagnosticCode value) noexcept {
    switch (value) {
    case DiagnosticCode::dsl_invalid_header:
        return "E_DSL_INVALID_HEADER";
    case DiagnosticCode::dsl_unexpected_token:
        return "E_DSL_UNEXPECTED_TOKEN";
    case DiagnosticCode::dsl_invalid_utf8:
        return "E_DSL_INVALID_UTF8";
    case DiagnosticCode::dsl_bom_not_allowed:
        return "E_DSL_BOM_NOT_ALLOWED";
    case DiagnosticCode::dsl_unterminated_string:
        return "E_DSL_UNTERMINATED_STRING";
    case DiagnosticCode::dsl_invalid_character:
        return "E_DSL_INVALID_CHARACTER";
    case DiagnosticCode::dsl_unknown_statement:
        return "E_DSL_UNKNOWN_STATEMENT";
    case DiagnosticCode::dsl_invalid_attribute:
        return "E_DSL_INVALID_ATTRIBUTE";
    case DiagnosticCode::dsl_duplicate_attribute:
        return "E_DSL_DUPLICATE_ATTRIBUTE";
    case DiagnosticCode::dsl_invalid_value:
        return "E_DSL_INVALID_VALUE";
    case DiagnosticCode::dsl_missing_required_field:
        return "E_DSL_MISSING_REQUIRED_FIELD";
    case DiagnosticCode::dsl_ambiguous_scope:
        return "E_DSL_AMBIGUOUS_SCOPE";
    case DiagnosticCode::include_invalid:
        return "E_INCLUDE_INVALID";
    case DiagnosticCode::include_escape:
        return "E_INCLUDE_ESCAPE";
    case DiagnosticCode::include_cycle:
        return "E_INCLUDE_CYCLE";
    case DiagnosticCode::include_depth:
        return "E_INCLUDE_DEPTH";
    case DiagnosticCode::include_io:
        return "E_INCLUDE_IO";
    case DiagnosticCode::id_invalid:
        return "E_ID_INVALID";
    case DiagnosticCode::id_duplicate:
        return "E_ID_DUPLICATE";
    case DiagnosticCode::reference_unresolved:
        return "E_REFERENCE_UNRESOLVED";
    case DiagnosticCode::endpoint_incompatible:
        return "E_ENDPOINT_INCOMPATIBLE";
    case DiagnosticCode::layer_incompatible:
        return "E_LAYER_INCOMPATIBLE";
    case DiagnosticCode::dependency_cycle:
        return "E_DEPENDENCY_CYCLE";
    case DiagnosticCode::limit_exceeded:
        return "E_LIMIT_EXCEEDED";
    case DiagnosticCode::limit_source_file_bytes:
        return "E_LIMIT_SOURCE_FILE_BYTES";
    case DiagnosticCode::limit_total_source_bytes:
        return "E_LIMIT_TOTAL_SOURCE_BYTES";
    case DiagnosticCode::limit_line_bytes:
        return "E_LIMIT_LINE_BYTES";
    case DiagnosticCode::observation_invalid:
        return "E_OBSERVATION_INVALID";
    }
    return "E_UNKNOWN";
}

} // namespace ltfa
