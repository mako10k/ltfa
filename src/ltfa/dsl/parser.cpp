#include "ltfa/dsl/parser.hpp"

#include <cstddef>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ltfa::dsl {
namespace {

struct LineParseResult {
    std::optional<Statement> statement;
    std::optional<Diagnostic> error;
};

[[nodiscard]] SourceSpan join_span(const Token &first, const Token &last) noexcept {
    return {first.source.file, first.source.begin, last.source.end};
}

[[nodiscard]] Diagnostic make_error(DiagnosticCode code, SourceSpan source, std::string message) {
    return {
        code, DiagnosticSeverity::error, source, std::nullopt, std::nullopt, std::move(message),
    };
}

[[nodiscard]] bool is_ascii_lower(char value) noexcept { return value >= 'a' && value <= 'z'; }

[[nodiscard]] bool is_ascii_digit(char value) noexcept { return value >= '0' && value <= '9'; }

[[nodiscard]] bool is_identifier(std::string_view text) noexcept {
    if (text.empty() || (!is_ascii_lower(text.front()) && !is_ascii_digit(text.front()))) {
        return false;
    }
    for (const char value : text) {
        if (!is_ascii_lower(value) && !is_ascii_digit(value) && value != '.' && value != '_' &&
            value != ':' && value != '-') {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool is_attribute_key(std::string_view text) noexcept {
    if (text.empty() || !is_ascii_lower(text.front())) {
        return false;
    }
    for (const char value : text) {
        if (!is_ascii_lower(value) && !is_ascii_digit(value) && value != '-') {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool is_integer(std::string_view text) noexcept {
    std::size_t offset = 0;
    if (!text.empty() && text.front() == '-') {
        offset = 1;
    }
    if (offset == text.size()) {
        return false;
    }
    for (; offset < text.size(); ++offset) {
        if (!is_ascii_digit(text[offset])) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool is_real(std::string_view text) noexcept {
    std::size_t offset = 0;
    if (!text.empty() && text.front() == '-') {
        offset = 1;
    }
    const auto dot = text.find('.', offset);
    if (dot == std::string_view::npos || dot == offset || dot + 1 == text.size() ||
        text.find('.', dot + 1) != std::string_view::npos) {
        return false;
    }
    for (std::size_t index = offset; index < text.size(); ++index) {
        if (index != dot && !is_ascii_digit(text[index])) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool is_duration(std::string_view text) noexcept {
    std::size_t offset = 0;
    while (offset < text.size() && is_ascii_digit(text[offset])) {
        ++offset;
    }
    if (offset == 0 || offset == text.size()) {
        return false;
    }
    const auto unit = text.substr(offset);
    return unit == "ms" || unit == "s" || unit == "m" || unit == "h" || unit == "d";
}

[[nodiscard]] bool is_one_of(std::string_view key,
                             std::initializer_list<std::string_view> keys) noexcept {
    for (const auto candidate : keys) {
        if (candidate == key) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool is_extension_key(std::string_view key) noexcept {
    return key.size() > 2 && key[0] == 'x' && key[1] == '-';
}

[[nodiscard]] bool is_known_attribute(StatementKind kind, std::string_view key) noexcept {
    switch (kind) {
    case StatementKind::scope:
        return is_one_of(key, {"kind", "label", "completeness", "evidence-quality"});
    case StatementKind::entity:
        return is_one_of(key, {"kind", "plane", "layer", "scope", "label", "network-space",
                               "failure-domain", "sensitivity", "source-ref", "address", "medium",
                               "protocol", "role"});
    case StatementKind::endpoint:
        return is_one_of(key, {"kind", "scope", "label", "network-space", "failure-domain",
                               "sensitivity", "source-ref", "address", "medium", "protocol", "role",
                               "normal-state", "current-range"});
    case StatementKind::connect:
    case StatementKind::relate:
        return is_one_of(key, {"kind", "label", "scope", "sensitivity", "source-ref"});
    case StatementKind::requirement:
        return is_one_of(key, {"group", "mode", "hold-up"});
    case StatementKind::failure:
        return is_one_of(key, {"mode", "after"});
    case StatementKind::probe:
        return is_one_of(key, {"kind", "valid-for"});
    case StatementKind::include:
        return false;
    }
    return false;
}

[[nodiscard]] bool is_repeatable_attribute(StatementKind kind, std::string_view key) noexcept {
    return key == "failure-domain" &&
           (kind == StatementKind::entity || kind == StatementKind::endpoint);
}

class LineParser {
  public:
    LineParser(const std::vector<Token> &tokens, const ModelLimits &limits)
        : tokens_(tokens), limits_(limits) {}

    [[nodiscard]] std::optional<ParsedValue> take_id(std::string_view description) {
        if (index_ >= tokens_.size()) {
            fail(DiagnosticCode::dsl_missing_required_field, end_source(),
                 "missing " + std::string{description});
            return std::nullopt;
        }
        const auto &token = tokens_[index_++];
        if ((token.kind != TokenKind::bare && token.kind != TokenKind::quoted_string) ||
            !is_identifier(token.text)) {
            fail(DiagnosticCode::dsl_invalid_value, token.source,
                 std::string{description} + " must match [a-z0-9][a-z0-9._:-]*");
            return std::nullopt;
        }
        return ParsedValue{ValueKind::identifier, token.text, token.source};
    }

    [[nodiscard]] std::optional<ParsedValue> take_named_id(std::string_view field) {
        if (!take_literal(field, DiagnosticCode::dsl_missing_required_field,
                          "missing required field " + std::string{field})) {
            return std::nullopt;
        }
        if (!take_kind(TokenKind::equals, DiagnosticCode::dsl_missing_required_field,
                       "required field " + std::string{field} + " must use '='")) {
            return std::nullopt;
        }
        return take_id(std::string{"value for "} + std::string{field});
    }

    [[nodiscard]] bool take_literal(std::string_view literal, DiagnosticCode code,
                                    std::string message) {
        if (index_ >= tokens_.size()) {
            fail(code, end_source(), std::move(message));
            return false;
        }
        const auto &token = tokens_[index_];
        if (token.kind != TokenKind::bare || token.text != literal) {
            fail(code, token.source, std::move(message));
            return false;
        }
        ++index_;
        return true;
    }

    [[nodiscard]] bool take_kind(TokenKind kind, DiagnosticCode code, std::string message) {
        if (index_ >= tokens_.size()) {
            fail(code, end_source(), std::move(message));
            return false;
        }
        const auto &token = tokens_[index_];
        if (token.kind != kind) {
            fail(code, token.source, std::move(message));
            return false;
        }
        ++index_;
        return true;
    }

    [[nodiscard]] std::optional<Direction> take_connection_direction() {
        if (index_ >= tokens_.size()) {
            fail(DiagnosticCode::dsl_missing_required_field, end_source(),
                 "connect requires '--' or '->' between objects");
            return std::nullopt;
        }
        const auto kind = tokens_[index_].kind;
        ++index_;
        if (kind == TokenKind::bidirectional_link) {
            return Direction::bidirectional;
        }
        if (kind == TokenKind::directed_arrow) {
            return Direction::directed;
        }
        fail(DiagnosticCode::dsl_unexpected_token, tokens_[index_ - 1].source,
             "connect requires '--' or '->' between objects");
        return std::nullopt;
    }

    [[nodiscard]] std::optional<std::vector<Attribute>>
    take_attributes(StatementKind statement_kind) {
        std::vector<Attribute> attributes;
        while (index_ < tokens_.size()) {
            if (attributes.size() >= limits_.max_attributes_per_object) {
                fail(DiagnosticCode::limit_exceeded, tokens_[index_].source,
                     "statement exceeds attribute limit of " +
                         std::to_string(limits_.max_attributes_per_object));
                return std::nullopt;
            }

            const auto &key_token = tokens_[index_++];
            if (key_token.kind != TokenKind::bare || !is_attribute_key(key_token.text)) {
                fail(DiagnosticCode::dsl_invalid_attribute, key_token.source,
                     "attribute key must match [a-z][a-z0-9-]*");
                return std::nullopt;
            }
            const bool extension = is_extension_key(key_token.text);
            if (!extension && !is_known_attribute(statement_kind, key_token.text)) {
                fail(DiagnosticCode::dsl_invalid_attribute, key_token.source,
                     "attribute '" + key_token.text + "' is not allowed on " +
                         std::string{to_string(statement_kind)} + " statements");
                return std::nullopt;
            }
            if (!take_kind(TokenKind::equals, DiagnosticCode::dsl_invalid_attribute,
                           "attribute '" + key_token.text + "' must use '='")) {
                return std::nullopt;
            }
            auto value = take_attribute_value(key_token.text);
            if (!value.has_value()) {
                return std::nullopt;
            }

            bool duplicate = false;
            for (const auto &existing : attributes) {
                if (existing.key == key_token.text) {
                    duplicate = true;
                    break;
                }
            }
            if (duplicate && !is_repeatable_attribute(statement_kind, key_token.text)) {
                fail(DiagnosticCode::dsl_duplicate_attribute, key_token.source,
                     "attribute '" + key_token.text + "' is duplicated");
                return std::nullopt;
            }

            const auto value_end = value->source.end;
            attributes.push_back({key_token.text,
                                  std::move(*value),
                                  extension,
                                  {key_token.source.file, key_token.source.begin, value_end}});
        }
        return attributes;
    }

    [[nodiscard]] const std::optional<Diagnostic> &error() const noexcept { return error_; }
    [[nodiscard]] SourceSpan source() const noexcept {
        return join_span(tokens_.front(), tokens_.back());
    }

  private:
    [[nodiscard]] std::optional<ParsedValue> take_attribute_value(std::string_view key) {
        if (index_ >= tokens_.size()) {
            fail(DiagnosticCode::dsl_missing_required_field, end_source(),
                 "attribute '" + std::string{key} + "' is missing a value");
            return std::nullopt;
        }
        const auto &token = tokens_[index_++];
        if (token.kind == TokenKind::quoted_string) {
            if (token.text.size() > limits_.max_attribute_string_bytes) {
                fail(DiagnosticCode::limit_exceeded, token.source,
                     "attribute string exceeds byte limit of " +
                         std::to_string(limits_.max_attribute_string_bytes));
                return std::nullopt;
            }
            return ParsedValue{ValueKind::string, token.text, token.source};
        }
        if (token.kind != TokenKind::bare) {
            fail(DiagnosticCode::dsl_invalid_value, token.source,
                 "attribute value must be an identifier, string, number, boolean, or duration");
            return std::nullopt;
        }

        ValueKind kind;
        if (token.text == "true" || token.text == "false") {
            kind = ValueKind::boolean;
        } else if (is_duration(token.text)) {
            kind = ValueKind::duration;
        } else if (is_integer(token.text)) {
            kind = ValueKind::integer;
        } else if (is_real(token.text)) {
            kind = ValueKind::real;
        } else if (is_identifier(token.text)) {
            kind = ValueKind::identifier;
        } else {
            fail(DiagnosticCode::dsl_invalid_value, token.source,
                 "attribute value must be an identifier, string, number, boolean, or duration");
            return std::nullopt;
        }
        return ParsedValue{kind, token.text, token.source};
    }

    [[nodiscard]] SourceSpan end_source() const noexcept {
        auto source = tokens_.back().source;
        source.begin = source.end;
        return source;
    }

    void fail(DiagnosticCode code, SourceSpan source, std::string message) {
        if (!error_.has_value()) {
            error_ = make_error(code, source, std::move(message));
        }
    }

    const std::vector<Token> &tokens_;
    const ModelLimits &limits_;
    std::size_t index_{1};
    std::optional<Diagnostic> error_;
};

[[nodiscard]] LineParseResult parse_statement(const std::vector<Token> &tokens,
                                              const ModelLimits &limits) {
    if (tokens.front().kind != TokenKind::bare) {
        return {std::nullopt,
                make_error(DiagnosticCode::dsl_unknown_statement, tokens.front().source,
                           "statement must begin with a DSL keyword")};
    }

    const auto keyword = tokens.front().text;
    LineParser parser{tokens, limits};
    const auto finish = [&parser](auto statement) -> LineParseResult {
        if (parser.error().has_value()) {
            return {std::nullopt, parser.error()};
        }
        return {Statement{std::move(statement)}, std::nullopt};
    };

    if (keyword == "scope") {
        auto id = parser.take_id("scope identifier");
        auto attributes = id.has_value() ? parser.take_attributes(StatementKind::scope)
                                         : std::optional<std::vector<Attribute>>{};
        if (!id.has_value() || !attributes.has_value()) {
            return {std::nullopt, parser.error()};
        }
        return finish(ScopeStatement{std::move(*id), std::move(*attributes), parser.source()});
    }
    if (keyword == "entity") {
        auto id = parser.take_id("entity identifier");
        auto attributes = id.has_value() ? parser.take_attributes(StatementKind::entity)
                                         : std::optional<std::vector<Attribute>>{};
        if (!id.has_value() || !attributes.has_value()) {
            return {std::nullopt, parser.error()};
        }
        return finish(EntityStatement{std::move(*id), std::move(*attributes), parser.source()});
    }
    if (keyword == "endpoint") {
        auto id = parser.take_id("endpoint identifier");
        auto owner = id.has_value() ? parser.take_named_id("owner") : std::optional<ParsedValue>{};
        auto attributes = owner.has_value() ? parser.take_attributes(StatementKind::endpoint)
                                            : std::optional<std::vector<Attribute>>{};
        if (!id.has_value() || !owner.has_value() || !attributes.has_value()) {
            return {std::nullopt, parser.error()};
        }
        return finish(EndpointStatement{std::move(*id), std::move(*owner), std::move(*attributes),
                                        parser.source()});
    }
    if (keyword == "connect") {
        auto from = parser.take_id("connect source object");
        auto direction =
            from.has_value() ? parser.take_connection_direction() : std::optional<Direction>{};
        auto to = direction.has_value() ? parser.take_id("connect target object")
                                        : std::optional<ParsedValue>{};
        auto attributes = to.has_value() ? parser.take_attributes(StatementKind::connect)
                                         : std::optional<std::vector<Attribute>>{};
        if (!from.has_value() || !direction.has_value() || !to.has_value() ||
            !attributes.has_value()) {
            return {std::nullopt, parser.error()};
        }
        return finish(ConnectStatement{std::move(*from), std::move(*to), *direction,
                                       std::move(*attributes), parser.source()});
    }
    if (keyword == "relate") {
        auto from = parser.take_id("relation source object");
        const bool arrow =
            from.has_value() &&
            parser.take_kind(TokenKind::directed_arrow, DiagnosticCode::dsl_unexpected_token,
                             "relate requires '->' between objects");
        auto to = arrow ? parser.take_id("relation target object") : std::optional<ParsedValue>{};
        auto attributes = to.has_value() ? parser.take_attributes(StatementKind::relate)
                                         : std::optional<std::vector<Attribute>>{};
        if (!from.has_value() || !arrow || !to.has_value() || !attributes.has_value()) {
            return {std::nullopt, parser.error()};
        }
        return finish(RelateStatement{std::move(*from), std::move(*to), std::move(*attributes),
                                      parser.source()});
    }
    if (keyword == "require") {
        auto consumer = parser.take_id("require consumer entity");
        const bool by = consumer.has_value() &&
                        parser.take_literal("by", DiagnosticCode::dsl_missing_required_field,
                                            "require needs 'by' before its provider");
        auto provider =
            by ? parser.take_id("require provider entity") : std::optional<ParsedValue>{};
        auto attributes = provider.has_value() ? parser.take_attributes(StatementKind::requirement)
                                               : std::optional<std::vector<Attribute>>{};
        if (!consumer.has_value() || !by || !provider.has_value() || !attributes.has_value()) {
            return {std::nullopt, parser.error()};
        }
        return finish(RequireStatement{std::move(*consumer), std::move(*provider),
                                       std::move(*attributes), parser.source()});
    }
    if (keyword == "failure") {
        auto id = parser.take_id("failure identifier");
        auto target =
            id.has_value() ? parser.take_named_id("target") : std::optional<ParsedValue>{};
        auto attributes = target.has_value() ? parser.take_attributes(StatementKind::failure)
                                             : std::optional<std::vector<Attribute>>{};
        if (!id.has_value() || !target.has_value() || !attributes.has_value()) {
            return {std::nullopt, parser.error()};
        }
        return finish(FailureStatement{std::move(*id), std::move(*target), std::move(*attributes),
                                       parser.source()});
    }
    if (keyword == "probe") {
        auto id = parser.take_id("probe identifier");
        auto target =
            id.has_value() ? parser.take_named_id("target") : std::optional<ParsedValue>{};
        auto vantage =
            target.has_value() ? parser.take_named_id("vantage") : std::optional<ParsedValue>{};
        auto attributes = vantage.has_value() ? parser.take_attributes(StatementKind::probe)
                                              : std::optional<std::vector<Attribute>>{};
        if (!id.has_value() || !target.has_value() || !vantage.has_value() ||
            !attributes.has_value()) {
            return {std::nullopt, parser.error()};
        }
        return finish(ProbeStatement{std::move(*id), std::move(*target), std::move(*vantage),
                                     std::move(*attributes), parser.source()});
    }
    if (keyword == "include") {
        if (tokens.size() != 2) {
            const auto location = tokens.size() > 2 ? tokens[2].source : parser.source();
            return {std::nullopt,
                    make_error(tokens.size() < 2 ? DiagnosticCode::dsl_missing_required_field
                                                 : DiagnosticCode::dsl_unexpected_token,
                               location, "include accepts exactly one quoted local path")};
        }
        const auto &path = tokens[1];
        if (path.kind != TokenKind::quoted_string) {
            return {std::nullopt, make_error(DiagnosticCode::dsl_invalid_value, path.source,
                                             "include path must be a quoted string")};
        }
        if (path.text.size() > limits.max_attribute_string_bytes) {
            return {std::nullopt,
                    make_error(DiagnosticCode::limit_exceeded, path.source,
                               "include path exceeds byte limit of " +
                                   std::to_string(limits.max_attribute_string_bytes))};
        }
        return {Statement{IncludeStatement{
                    ParsedValue{ValueKind::string, path.text, path.source},
                    parser.source(),
                }},
                std::nullopt};
    }

    return {std::nullopt, make_error(DiagnosticCode::dsl_unknown_statement, tokens.front().source,
                                     "unknown DSL statement '" + keyword + "'")};
}

} // namespace

std::string_view to_string(StatementKind value) noexcept {
    switch (value) {
    case StatementKind::scope:
        return "scope";
    case StatementKind::entity:
        return "entity";
    case StatementKind::endpoint:
        return "endpoint";
    case StatementKind::connect:
        return "connect";
    case StatementKind::relate:
        return "relate";
    case StatementKind::requirement:
        return "require";
    case StatementKind::failure:
        return "failure";
    case StatementKind::probe:
        return "probe";
    case StatementKind::include:
        return "include";
    }
    return "unknown";
}

std::string_view to_string(ValueKind value) noexcept {
    switch (value) {
    case ValueKind::identifier:
        return "identifier";
    case ValueKind::string:
        return "string";
    case ValueKind::integer:
        return "integer";
    case ValueKind::real:
        return "real";
    case ValueKind::boolean:
        return "boolean";
    case ValueKind::duration:
        return "duration";
    }
    return "unknown";
}

ParseResult parse_document(std::string_view source, StringId file, const ModelLimits &limits) {
    auto lexed = lex_document(source, file, limits);
    if (!lexed.ok()) {
        return {std::nullopt, std::move(lexed.error)};
    }

    std::size_t header_index = 0;
    while (header_index < lexed.lines.size() && lexed.lines[header_index].tokens.empty()) {
        ++header_index;
    }
    if (header_index == lexed.lines.size()) {
        const SourceSpan location{file, {0, 1, 1}, {0, 1, 1}};
        return {std::nullopt, make_error(DiagnosticCode::dsl_invalid_header, location,
                                         "document must begin with 'ltfa 1'")};
    }

    const auto &header = lexed.lines[header_index].tokens;
    const auto header_source = join_span(header.front(), header.back());
    if (header.size() != 2 || header[0].kind != TokenKind::bare || header[0].text != "ltfa" ||
        header[1].kind != TokenKind::bare || header[1].text != "1") {
        return {std::nullopt, make_error(DiagnosticCode::dsl_invalid_header, header_source,
                                         "document header must be exactly 'ltfa 1'")};
    }

    Document document;
    document.header_source = header_source;
    for (std::size_t index = header_index + 1; index < lexed.lines.size(); ++index) {
        const auto &tokens = lexed.lines[index].tokens;
        if (tokens.empty()) {
            continue;
        }
        auto parsed = parse_statement(tokens, limits);
        if (parsed.error.has_value()) {
            return {std::nullopt, std::move(parsed.error)};
        }
        document.statements.push_back(std::move(*parsed.statement));
    }
    return {std::move(document), std::nullopt};
}

} // namespace ltfa::dsl
