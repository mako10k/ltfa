#include "ltfa/dsl/parser.hpp"
#include "ltfa/model_compiler.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

int failures = 0;
constexpr ltfa::StringId kFile{17};

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << message << '\n';
        ++failures;
    }
}

void expect_location(const ltfa::Diagnostic &diagnostic, std::uint32_t line, std::uint32_t column,
                     std::string_view description) {
    if (diagnostic.source.file != kFile || diagnostic.source.begin.line != line ||
        diagnostic.source.begin.column != column) {
        std::cerr << description << ": expected " << line << ':' << column << ", got "
                  << diagnostic.source.begin.line << ':' << diagnostic.source.begin.column << '\n';
        ++failures;
    }
}

std::optional<ltfa::Diagnostic> parse_error(std::string_view source,
                                            ltfa::DiagnosticCode expected) {
    const auto result = ltfa::dsl::parse_document(source, kFile);
    if (result.document.has_value()) {
        std::cerr << "malformed parser input exposed a partial document\n";
        ++failures;
    }
    if (!result.error.has_value()) {
        std::cerr << "expected parser diagnostic " << ltfa::to_string(expected) << '\n';
        ++failures;
        return std::nullopt;
    }
    if (result.error->code != expected) {
        std::cerr << "expected parser diagnostic " << ltfa::to_string(expected) << ", got "
                  << ltfa::to_string(result.error->code) << '\n';
        ++failures;
    }
    return result.error;
}

std::optional<ltfa::Diagnostic>
compile_error(std::string_view source, ltfa::DiagnosticCode expected,
              const ltfa::ModelLimits &limits = ltfa::kDefaultModelLimits) {
    auto parsed = ltfa::dsl::parse_document(source, kFile, limits);
    if (!parsed.ok()) {
        std::cerr << "compiler fixture failed to parse";
        if (parsed.error.has_value()) {
            std::cerr << ": " << ltfa::to_string(parsed.error->code);
        }
        std::cerr << '\n';
        ++failures;
        return std::nullopt;
    }

    ltfa::dsl::LoadedDocument loaded;
    loaded.statements = std::move(parsed.document->statements);
    loaded.total_source_bytes = source.size();
    auto result = ltfa::compile_model(loaded, limits);
    if (result.model.has_value()) {
        std::cerr << "invalid compiler input exposed a partial model\n";
        ++failures;
    }
    if (!result.error.has_value()) {
        std::cerr << "expected compiler diagnostic " << ltfa::to_string(expected) << '\n';
        ++failures;
        return std::nullopt;
    }
    if (result.error->code != expected) {
        std::cerr << "expected compiler diagnostic " << ltfa::to_string(expected) << ", got "
                  << ltfa::to_string(result.error->code) << '\n';
        ++failures;
    }
    return result.error;
}

bool compiles(std::string_view source, const ltfa::ModelLimits &limits) {
    auto parsed = ltfa::dsl::parse_document(source, kFile, limits);
    if (!parsed.ok()) {
        return false;
    }
    ltfa::dsl::LoadedDocument loaded;
    loaded.statements = std::move(parsed.document->statements);
    loaded.total_source_bytes = source.size();
    return ltfa::compile_model(loaded, limits).ok();
}

void test_malformed_parser_matrix() {
    struct Case {
        std::string source;
        ltfa::DiagnosticCode code;
        std::uint32_t line;
        std::uint32_t column;
    };

    std::string invalid_control = "ltfa 1\nscope s";
    invalid_control.push_back('\0');
    invalid_control.push_back('\n');
    std::string invalid_utf8 = "ltfa 1\nscope ";
    invalid_utf8.push_back(static_cast<char>(0xC0));
    invalid_utf8.push_back(static_cast<char>(0xAF));
    invalid_utf8.push_back('\n');

    const std::vector<Case> cases{
        {"", ltfa::DiagnosticCode::dsl_invalid_header, 1, 1},
        {"ltfa 2\n", ltfa::DiagnosticCode::dsl_invalid_header, 1, 1},
        {"ltfa 1\nscope s label=\"unterminated\n", ltfa::DiagnosticCode::dsl_unterminated_string, 2,
         15},
        {std::move(invalid_control), ltfa::DiagnosticCode::dsl_invalid_character, 2, 8},
        {std::move(invalid_utf8), ltfa::DiagnosticCode::dsl_invalid_utf8, 2, 7},
        {"ltfa 1\nunknown x\n", ltfa::DiagnosticCode::dsl_unknown_statement, 2, 1},
        {"ltfa 1\nscope s kind=site kind=rack\n", ltfa::DiagnosticCode::dsl_duplicate_attribute, 2,
         19},
        {"ltfa 1\nentity \"$(cmd)\"\n", ltfa::DiagnosticCode::dsl_invalid_value, 2, 8},
        {"ltfa 1\nendpoint p kind=network\n", ltfa::DiagnosticCode::dsl_missing_required_field, 2,
         12},
        {"ltfa 1\nrelate a -- b kind=network\n", ltfa::DiagnosticCode::dsl_unexpected_token, 2, 10},
    };

    for (const auto &test_case : cases) {
        const auto first = parse_error(test_case.source, test_case.code);
        const auto second = parse_error(test_case.source, test_case.code);
        if (!first.has_value() || !second.has_value()) {
            continue;
        }
        expect_location(*first, test_case.line, test_case.column, "parser matrix location");
        expect(first->code == second->code &&
                   first->source.begin.line == second->source.begin.line &&
                   first->source.begin.column == second->source.begin.column &&
                   first->message == second->message,
               "malformed parser diagnostics must be deterministic");
    }
}

void test_invalid_model_matrix() {
    struct Case {
        std::string source;
        ltfa::DiagnosticCode code;
        std::uint32_t line;
        std::uint32_t column;
    };
    const std::string prefix = "ltfa 1\nscope s\n";
    const std::vector<Case> cases{
        {prefix + "entity same scope=s\nentity same scope=s\n", ltfa::DiagnosticCode::id_duplicate,
         4, 8},
        {prefix + "entity e scope=s\nendpoint p owner=missing kind=network\n",
         ltfa::DiagnosticCode::reference_unresolved, 4, 18},
        {prefix + "entity source kind=facility plane=facility layer=physical scope=s\n"
                  "entity sink kind=device plane=communication layer=physical scope=s\n"
                  "endpoint out owner=source kind=power-output\n"
                  "endpoint in owner=sink kind=network\n"
                  "connect out -- in kind=power\n",
         ltfa::DiagnosticCode::endpoint_incompatible, 7, 1},
        {prefix + "entity a kind=device plane=communication layer=l3 scope=s\n"
                  "entity b kind=device plane=communication layer=l3 scope=s\n"
                  "relate a -> b kind=depends_on\n"
                  "relate b -> a kind=depends_on\n",
         ltfa::DiagnosticCode::dependency_cycle, 6, 1},
    };

    for (const auto &test_case : cases) {
        const auto first = compile_error(test_case.source, test_case.code);
        const auto second = compile_error(test_case.source, test_case.code);
        if (!first.has_value() || !second.has_value()) {
            continue;
        }
        expect_location(*first, test_case.line, test_case.column, "compiler matrix location");
        expect(first->code == second->code &&
                   first->source.begin.line == second->source.begin.line &&
                   first->source.begin.column == second->source.begin.column &&
                   first->message == second->message,
               "invalid model diagnostics must be deterministic");
    }
}

void test_limit_boundaries() {
    constexpr std::string_view minimal = "ltfa 1\n";
    auto limits = ltfa::kDefaultModelLimits;
    limits.max_source_file_bytes = minimal.size();
    limits.max_total_source_bytes = minimal.size();
    limits.max_line_bytes = 6;
    expect(compiles(minimal, limits), "exact source and line byte limits must be accepted");

    limits.max_source_file_bytes = minimal.size() - 1;
    const auto source_limit = ltfa::dsl::parse_document(minimal, kFile, limits);
    expect(source_limit.error.has_value() &&
               source_limit.error->code == ltfa::DiagnosticCode::limit_source_file_bytes &&
               !source_limit.document.has_value(),
           "source byte limit must fail closed at limit plus one");

    limits = ltfa::kDefaultModelLimits;
    limits.max_line_bytes = 5;
    const auto line_limit = ltfa::dsl::parse_document(minimal, kFile, limits);
    expect(line_limit.error.has_value() &&
               line_limit.error->code == ltfa::DiagnosticCode::limit_line_bytes &&
               !line_limit.document.has_value(),
           "line byte limit must fail closed at limit plus one");

    const std::string attributes = "ltfa 1\nscope s kind=site label=\"four\"\n";
    limits = ltfa::kDefaultModelLimits;
    limits.max_attributes_per_object = 2;
    limits.max_attribute_string_bytes = 4;
    expect(compiles(attributes, limits), "exact attribute count and byte limits must be accepted");

    limits.max_attributes_per_object = 1;
    auto result = ltfa::dsl::parse_document(attributes, kFile, limits);
    expect(result.error.has_value() && result.error->code == ltfa::DiagnosticCode::limit_exceeded &&
               !result.document.has_value(),
           "attribute count must fail closed at limit plus one");

    limits = ltfa::kDefaultModelLimits;
    limits.max_attribute_string_bytes = 3;
    result = ltfa::dsl::parse_document(attributes, kFile, limits);
    expect(result.error.has_value() && result.error->code == ltfa::DiagnosticCode::limit_exceeded &&
               !result.document.has_value(),
           "attribute string must fail closed at limit plus one");

    const std::string graph = "ltfa 1\n"
                              "scope s\n"
                              "entity a kind=device plane=communication layer=physical scope=s\n"
                              "entity b kind=device plane=communication layer=physical scope=s\n"
                              "entity c kind=device plane=communication layer=physical scope=s\n"
                              "endpoint a-p owner=a kind=network\n"
                              "endpoint b-p owner=b kind=network\n"
                              "endpoint c-p owner=c kind=network\n"
                              "connect a-p -- b-p kind=network\n"
                              "connect b-p -- c-p kind=network\n";
    limits = ltfa::kDefaultModelLimits;
    limits.max_entities = 6;
    limits.max_relations = 2;
    limits.max_total_source_bytes = graph.size();
    expect(compiles(graph, limits),
           "exact object, relation, and total byte limits must be accepted");

    limits.max_entities = 5;
    compile_error(graph, ltfa::DiagnosticCode::limit_exceeded, limits);
    limits.max_entities = 6;
    limits.max_relations = 1;
    compile_error(graph, ltfa::DiagnosticCode::limit_exceeded, limits);
    limits.max_relations = 2;
    limits.max_total_source_bytes = graph.size() - 1;
    compile_error(graph, ltfa::DiagnosticCode::limit_total_source_bytes, limits);
}

} // namespace

int main() {
    test_malformed_parser_matrix();
    test_invalid_model_matrix();
    test_limit_boundaries();
    return failures == 0 ? 0 : 1;
}
