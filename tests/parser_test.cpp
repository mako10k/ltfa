#include "ltfa/dsl/parser.hpp"

#include <array>
#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

namespace {

int failures = 0;
constexpr ltfa::StringId kFile{2};

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << message << '\n';
        ++failures;
    }
}

const ltfa::Diagnostic *expect_error(const ltfa::dsl::ParseResult &result,
                                     ltfa::DiagnosticCode code) {
    expect(!result.document.has_value(), "failed parsing must not expose a partial document");
    if (!result.error.has_value()) {
        std::cerr << "expected diagnostic " << ltfa::to_string(code) << '\n';
        ++failures;
        return nullptr;
    }
    if (result.error->code != code) {
        std::cerr << "expected diagnostic " << ltfa::to_string(code) << ", got "
                  << ltfa::to_string(result.error->code) << '\n';
        ++failures;
    }
    return &*result.error;
}

const ltfa::dsl::Attribute *find_attribute(const std::vector<ltfa::dsl::Attribute> &attributes,
                                           std::string_view key, std::size_t occurrence = 0) {
    for (const auto &attribute : attributes) {
        if (attribute.key == key) {
            if (occurrence == 0) {
                return &attribute;
            }
            --occurrence;
        }
    }
    return nullptr;
}

void test_enum_names() {
    const std::array statement_names{
        std::pair{ltfa::dsl::StatementKind::scope, std::string_view{"scope"}},
        std::pair{ltfa::dsl::StatementKind::entity, std::string_view{"entity"}},
        std::pair{ltfa::dsl::StatementKind::endpoint, std::string_view{"endpoint"}},
        std::pair{ltfa::dsl::StatementKind::connect, std::string_view{"connect"}},
        std::pair{ltfa::dsl::StatementKind::relate, std::string_view{"relate"}},
        std::pair{ltfa::dsl::StatementKind::requirement, std::string_view{"require"}},
        std::pair{ltfa::dsl::StatementKind::failure, std::string_view{"failure"}},
        std::pair{ltfa::dsl::StatementKind::probe, std::string_view{"probe"}},
        std::pair{ltfa::dsl::StatementKind::include, std::string_view{"include"}},
    };
    for (const auto &[value, name] : statement_names) {
        expect(ltfa::dsl::to_string(value) == name, "statement name must remain stable");
    }

    const std::array value_names{
        std::pair{ltfa::dsl::ValueKind::identifier, std::string_view{"identifier"}},
        std::pair{ltfa::dsl::ValueKind::string, std::string_view{"string"}},
        std::pair{ltfa::dsl::ValueKind::integer, std::string_view{"integer"}},
        std::pair{ltfa::dsl::ValueKind::real, std::string_view{"real"}},
        std::pair{ltfa::dsl::ValueKind::boolean, std::string_view{"boolean"}},
        std::pair{ltfa::dsl::ValueKind::duration, std::string_view{"duration"}},
    };
    for (const auto &[value, name] : value_names) {
        expect(ltfa::dsl::to_string(value) == name, "value kind name must remain stable");
    }
}

void test_all_statement_shapes() {
    const std::string source =
        "# leading comment\n"
        "ltfa 1\n"
        "\n"
        "scope \"site:ami-a\" kind=site label=\"阿見A\" completeness=partial "
        "evidence-quality=confirmed x-owner=\"ops\"\n"
        "entity \"device:ami-a:router\" kind=device plane=communication layer=physical "
        "failure-domain=panel-a failure-domain=cabinet-a protocol=modbus-rtu\n"
        "endpoint \"endpoint:ami-a:router:wan\" owner=\"device:ami-a:router\" kind=network "
        "medium=ethernet\n"
        "connect \"endpoint:ami-a:router:wan\" -- \"network:internet\" kind=network\n"
        "connect \"power:panel\" -> \"endpoint:ami-a:router:psu\" kind=power\n"
        "relate \"overlay:ami-a:vpn\" -> \"network:internet\" kind=uses-underlay\n"
        "require \"service:remote-monitoring\" by \"cloud:vnet-production\" "
        "group=connectivity mode=all hold-up=30s\n"
        "failure \"failure:ami-a:panel-loss\" target=\"power:panel\" mode=power-loss after=0s\n"
        "probe \"probe:router:ping\" target=\"endpoint:ami-a:router:wan\" "
        "vantage=\"node:cloud-gateway\" kind=icmp valid-for=60s\n"
        "include \"models/site-b.ltfa\"\n";

    const auto result = ltfa::dsl::parse_document(source, kFile);
    expect(result.ok(), "complete DSL v1 example must parse");
    if (!result.ok()) {
        if (result.error.has_value()) {
            std::cerr << ltfa::to_string(result.error->code) << ": " << result.error->message
                      << '\n';
        }
        return;
    }

    const auto &document = *result.document;
    expect(document.major_version == 1, "header must retain major version 1");
    expect(document.header_source.begin.line == 2, "header source line must skip comments");
    expect(document.statements.size() == 10, "all statement records must be retained");

    const auto *scope = std::get_if<ltfa::dsl::ScopeStatement>(&document.statements[0]);
    expect(scope != nullptr, "scope must use its typed record");
    if (scope != nullptr) {
        expect(scope->id.text == "site:ami-a", "quoted scope ID must retain its text");
        const auto *extension = find_attribute(scope->attributes, "x-owner");
        expect(extension != nullptr && extension->extension,
               "x- attribute must be marked and preserved");
        expect(scope->source.begin.line == 4, "statement span must retain its line");
    }

    const auto *entity = std::get_if<ltfa::dsl::EntityStatement>(&document.statements[1]);
    expect(entity != nullptr, "entity must use its typed record");
    if (entity != nullptr) {
        expect(find_attribute(entity->attributes, "failure-domain", 1) != nullptr,
               "repeatable failure-domain attributes must be retained");
    }

    const auto *endpoint = std::get_if<ltfa::dsl::EndpointStatement>(&document.statements[2]);
    expect(endpoint != nullptr && endpoint->owner.text == "device:ami-a:router",
           "endpoint owner must be a typed required field");

    const auto *bidirectional = std::get_if<ltfa::dsl::ConnectStatement>(&document.statements[3]);
    const auto *directed = std::get_if<ltfa::dsl::ConnectStatement>(&document.statements[4]);
    expect(bidirectional != nullptr && bidirectional->direction == ltfa::Direction::bidirectional,
           "'--' must retain bidirectional semantics");
    expect(directed != nullptr && directed->direction == ltfa::Direction::directed,
           "'->' must retain directed semantics");

    expect(std::holds_alternative<ltfa::dsl::RelateStatement>(document.statements[5]),
           "relate must use its typed record");
    const auto *requirement = std::get_if<ltfa::dsl::RequireStatement>(&document.statements[6]);
    expect(requirement != nullptr && requirement->provider.text == "cloud:vnet-production",
           "require provider must be retained");
    const auto *failure = std::get_if<ltfa::dsl::FailureStatement>(&document.statements[7]);
    expect(failure != nullptr && failure->target.text == "power:panel",
           "failure target must be retained");
    const auto *probe = std::get_if<ltfa::dsl::ProbeStatement>(&document.statements[8]);
    expect(probe != nullptr && probe->vantage.text == "node:cloud-gateway",
           "probe vantage must be retained");
    const auto *include = std::get_if<ltfa::dsl::IncludeStatement>(&document.statements[9]);
    expect(include != nullptr && include->path.text == "models/site-b.ltfa",
           "include path must be preserved without resolving it");
}

void test_attribute_value_types() {
    const std::string source = "ltfa 1\n"
                               "scope s x-id=alpha x-string=\"two words\" x-integer=-2 x-real=1.5 "
                               "x-boolean=true x-duration=60s\n";
    const auto result = ltfa::dsl::parse_document(source, kFile);
    expect(result.ok(), "all DSL attribute value kinds must parse");
    if (!result.ok()) {
        return;
    }
    const auto *scope = std::get_if<ltfa::dsl::ScopeStatement>(&result.document->statements[0]);
    if (scope == nullptr) {
        expect(false, "value type fixture must produce a scope");
        return;
    }

    const std::array expected{
        std::pair{std::string_view{"x-id"}, ltfa::dsl::ValueKind::identifier},
        std::pair{std::string_view{"x-string"}, ltfa::dsl::ValueKind::string},
        std::pair{std::string_view{"x-integer"}, ltfa::dsl::ValueKind::integer},
        std::pair{std::string_view{"x-real"}, ltfa::dsl::ValueKind::real},
        std::pair{std::string_view{"x-boolean"}, ltfa::dsl::ValueKind::boolean},
        std::pair{std::string_view{"x-duration"}, ltfa::dsl::ValueKind::duration},
    };
    for (const auto &[key, kind] : expected) {
        const auto *attribute = find_attribute(scope->attributes, key);
        expect(attribute != nullptr && attribute->value.kind == kind,
               "attribute value must retain its lexical type");
        if (attribute != nullptr) {
            expect(attribute->source.begin.byte_offset <= attribute->value.source.begin.byte_offset,
                   "attribute span must cover its value");
        }
    }
}

void test_header_and_statement_failures() {
    expect_error(ltfa::dsl::parse_document("", kFile), ltfa::DiagnosticCode::dsl_invalid_header);
    expect_error(ltfa::dsl::parse_document("ltfa 2\n", kFile),
                 ltfa::DiagnosticCode::dsl_invalid_header);
    expect_error(ltfa::dsl::parse_document("ltfa 1 extra\n", kFile),
                 ltfa::DiagnosticCode::dsl_invalid_header);
    expect_error(ltfa::dsl::parse_document("scope s\n", kFile),
                 ltfa::DiagnosticCode::dsl_invalid_header);
    expect_error(ltfa::dsl::parse_document("ltfa 1\nunknown x\n", kFile),
                 ltfa::DiagnosticCode::dsl_unknown_statement);
    expect_error(ltfa::dsl::parse_document("ltfa 1\nscope s arbitrary=value\n", kFile),
                 ltfa::DiagnosticCode::dsl_invalid_attribute);
    expect_error(ltfa::dsl::parse_document("ltfa 1\nscope s label=a label=b\n", kFile),
                 ltfa::DiagnosticCode::dsl_duplicate_attribute);
    expect_error(ltfa::dsl::parse_document("ltfa 1\nscope s x-v=${HOME}\n", kFile),
                 ltfa::DiagnosticCode::dsl_invalid_value);
    expect_error(ltfa::dsl::parse_document("ltfa 1\nentity \"$(cmd)\"\n", kFile),
                 ltfa::DiagnosticCode::dsl_invalid_value);
    expect_error(ltfa::dsl::parse_document("ltfa 1\nendpoint e kind=network\n", kFile),
                 ltfa::DiagnosticCode::dsl_missing_required_field);
    expect_error(ltfa::dsl::parse_document("ltfa 1\nrelate a -- b kind=network\n", kFile),
                 ltfa::DiagnosticCode::dsl_unexpected_token);
    expect_error(ltfa::dsl::parse_document("ltfa 1\ninclude path.ltfa\n", kFile),
                 ltfa::DiagnosticCode::dsl_invalid_value);
    expect_error(ltfa::dsl::parse_document("ltfa 1\ninclude \"a\" \"b\"\n", kFile),
                 ltfa::DiagnosticCode::dsl_unexpected_token);
}

void test_limits_and_lexer_errors_propagate() {
    ltfa::ModelLimits limits = ltfa::kDefaultModelLimits;
    limits.max_attributes_per_object = 1;
    expect_error(ltfa::dsl::parse_document("ltfa 1\nscope s kind=site label=a\n", kFile, limits),
                 ltfa::DiagnosticCode::limit_exceeded);

    limits = ltfa::kDefaultModelLimits;
    limits.max_attribute_string_bytes = 3;
    expect_error(ltfa::dsl::parse_document("ltfa 1\nscope s label=\"four\"\n", kFile, limits),
                 ltfa::DiagnosticCode::limit_exceeded);
    expect_error(ltfa::dsl::parse_document("ltfa 1\ninclude \"four\"\n", kFile, limits),
                 ltfa::DiagnosticCode::limit_exceeded);

    limits = ltfa::kDefaultModelLimits;
    limits.max_line_bytes = 5;
    expect_error(ltfa::dsl::parse_document("ltfa 1\n", kFile, limits),
                 ltfa::DiagnosticCode::limit_line_bytes);
}

} // namespace

int main() {
    test_enum_names();
    test_all_statement_shapes();
    test_attribute_value_types();
    test_header_and_statement_failures();
    test_limits_and_lexer_errors_propagate();
    return failures == 0 ? 0 : 1;
}
