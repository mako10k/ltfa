#pragma once

#include "ltfa/dsl/lexer.hpp"
#include "ltfa/model.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace ltfa::dsl {

enum class StatementKind {
    scope,
    entity,
    endpoint,
    connect,
    relate,
    requirement,
    failure,
    probe,
    include,
};

enum class ValueKind {
    identifier,
    string,
    integer,
    real,
    boolean,
    duration,
};

[[nodiscard]] std::string_view to_string(StatementKind value) noexcept;
[[nodiscard]] std::string_view to_string(ValueKind value) noexcept;

struct ParsedValue {
    ValueKind kind{ValueKind::identifier};
    std::string text;
    SourceSpan source;
};

struct Attribute {
    std::string key;
    ParsedValue value;
    bool extension{false};
    SourceSpan source;
};

struct ScopeStatement {
    ParsedValue id;
    std::vector<Attribute> attributes;
    SourceSpan source;
};

struct EntityStatement {
    ParsedValue id;
    std::vector<Attribute> attributes;
    SourceSpan source;
};

struct EndpointStatement {
    ParsedValue id;
    ParsedValue owner;
    std::vector<Attribute> attributes;
    SourceSpan source;
};

struct ConnectStatement {
    ParsedValue from;
    ParsedValue to;
    Direction direction{Direction::bidirectional};
    std::vector<Attribute> attributes;
    SourceSpan source;
};

struct RelateStatement {
    ParsedValue from;
    ParsedValue to;
    std::vector<Attribute> attributes;
    SourceSpan source;
};

struct RequireStatement {
    ParsedValue consumer;
    ParsedValue provider;
    std::vector<Attribute> attributes;
    SourceSpan source;
};

struct FailureStatement {
    ParsedValue id;
    ParsedValue target;
    std::vector<Attribute> attributes;
    SourceSpan source;
};

struct ProbeStatement {
    ParsedValue id;
    ParsedValue target;
    ParsedValue vantage;
    std::vector<Attribute> attributes;
    SourceSpan source;
};

struct IncludeStatement {
    ParsedValue path;
    SourceSpan source;
};

using Statement = std::variant<ScopeStatement, EntityStatement, EndpointStatement, ConnectStatement,
                               RelateStatement, RequireStatement, FailureStatement, ProbeStatement,
                               IncludeStatement>;

struct Document {
    std::uint32_t major_version{1};
    SourceSpan header_source;
    std::vector<Statement> statements;
};

struct ParseResult {
    std::optional<Document> document;
    std::optional<Diagnostic> error;

    [[nodiscard]] bool ok() const noexcept { return document.has_value() && !error.has_value(); }
};

[[nodiscard]] ParseResult parse_document(std::string_view source, StringId file,
                                         const ModelLimits &limits = kDefaultModelLimits);

} // namespace ltfa::dsl
