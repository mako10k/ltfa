#include "ltfa/dsl/lexer.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

namespace ltfa::dsl {
namespace {

struct Utf8Sequence {
    std::size_t length{1};
    bool valid{false};
};

struct LineResult {
    std::vector<Token> tokens;
    std::optional<Diagnostic> error;
};

[[nodiscard]] bool is_continuation(unsigned char value) noexcept {
    return value >= 0x80U && value <= 0xBFU;
}

[[nodiscard]] Utf8Sequence inspect_utf8(std::string_view text, std::size_t offset) noexcept {
    const auto first = static_cast<unsigned char>(text[offset]);
    if (first <= 0x7FU) {
        return {1, true};
    }
    if (first >= 0xC2U && first <= 0xDFU) {
        if (offset + 1 < text.size() &&
            is_continuation(static_cast<unsigned char>(text[offset + 1]))) {
            return {2, true};
        }
        return {};
    }
    if (first == 0xE0U) {
        if (offset + 2 < text.size()) {
            const auto second = static_cast<unsigned char>(text[offset + 1]);
            const auto third = static_cast<unsigned char>(text[offset + 2]);
            if (second >= 0xA0U && second <= 0xBFU && is_continuation(third)) {
                return {3, true};
            }
        }
        return {};
    }
    if ((first >= 0xE1U && first <= 0xECU) || (first >= 0xEEU && first <= 0xEFU)) {
        if (offset + 2 < text.size() &&
            is_continuation(static_cast<unsigned char>(text[offset + 1])) &&
            is_continuation(static_cast<unsigned char>(text[offset + 2]))) {
            return {3, true};
        }
        return {};
    }
    if (first == 0xEDU) {
        if (offset + 2 < text.size()) {
            const auto second = static_cast<unsigned char>(text[offset + 1]);
            const auto third = static_cast<unsigned char>(text[offset + 2]);
            if (second >= 0x80U && second <= 0x9FU && is_continuation(third)) {
                return {3, true};
            }
        }
        return {};
    }
    if (first == 0xF0U) {
        if (offset + 3 < text.size()) {
            const auto second = static_cast<unsigned char>(text[offset + 1]);
            const auto third = static_cast<unsigned char>(text[offset + 2]);
            const auto fourth = static_cast<unsigned char>(text[offset + 3]);
            if (second >= 0x90U && second <= 0xBFU && is_continuation(third) &&
                is_continuation(fourth)) {
                return {4, true};
            }
        }
        return {};
    }
    if (first >= 0xF1U && first <= 0xF3U) {
        if (offset + 3 < text.size() &&
            is_continuation(static_cast<unsigned char>(text[offset + 1])) &&
            is_continuation(static_cast<unsigned char>(text[offset + 2])) &&
            is_continuation(static_cast<unsigned char>(text[offset + 3]))) {
            return {4, true};
        }
        return {};
    }
    if (first == 0xF4U) {
        if (offset + 3 < text.size()) {
            const auto second = static_cast<unsigned char>(text[offset + 1]);
            const auto third = static_cast<unsigned char>(text[offset + 2]);
            const auto fourth = static_cast<unsigned char>(text[offset + 3]);
            if (second >= 0x80U && second <= 0x8FU && is_continuation(third) &&
                is_continuation(fourth)) {
                return {4, true};
            }
        }
        return {};
    }
    return {};
}

[[nodiscard]] std::optional<std::size_t> first_invalid_utf8(std::string_view text) noexcept {
    std::size_t offset = 0;
    while (offset < text.size()) {
        const auto sequence = inspect_utf8(text, offset);
        if (!sequence.valid) {
            return offset;
        }
        offset += sequence.length;
    }
    return std::nullopt;
}

[[nodiscard]] std::uint32_t column_at(std::string_view line, std::size_t byte_offset) noexcept {
    std::size_t offset = 0;
    std::uint32_t column = 1;
    while (offset < byte_offset) {
        const auto sequence = inspect_utf8(line, offset);
        offset += sequence.valid ? sequence.length : 1;
        ++column;
    }
    return column;
}

[[nodiscard]] SourceSpan make_span(StringId file, std::uint32_t line_number,
                                   std::uint64_t line_offset, std::size_t begin_byte,
                                   std::uint32_t begin_column, std::size_t end_byte,
                                   std::uint32_t end_column) noexcept {
    return {
        file,
        {line_offset + begin_byte, line_number, begin_column},
        {line_offset + end_byte, line_number, end_column},
    };
}

[[nodiscard]] Diagnostic make_error(DiagnosticCode code, SourceSpan source, std::string message) {
    return {
        code, DiagnosticSeverity::error, source, std::nullopt, std::nullopt, std::move(message),
    };
}

[[nodiscard]] bool is_horizontal_space(char value) noexcept {
    return value == ' ' || value == '\t';
}

[[nodiscard]] bool is_invalid_ascii_control(unsigned char value) noexcept {
    return (value < 0x20U && value != static_cast<unsigned char>('\t')) || value == 0x7FU;
}

[[nodiscard]] bool starts_operator(std::string_view line, std::size_t offset) noexcept {
    return offset + 1 < line.size() && line[offset] == '-' &&
           (line[offset + 1] == '>' || line[offset + 1] == '-');
}

[[nodiscard]] LineResult fail_line(Diagnostic diagnostic) { return {{}, std::move(diagnostic)}; }

[[nodiscard]] LineResult lex_line(std::string_view line, StringId file, std::uint32_t line_number,
                                  std::uint64_t line_offset, const ModelLimits &limits) {
    if (const auto invalid = first_invalid_utf8(line); invalid.has_value()) {
        const auto column = column_at(line, *invalid);
        return fail_line(make_error(
            DiagnosticCode::dsl_invalid_utf8,
            make_span(file, line_number, line_offset, *invalid, column, *invalid + 1, column + 1),
            "invalid UTF-8 byte sequence"));
    }

    if (line.size() > limits.max_line_bytes) {
        const auto end_column = column_at(line, line.size());
        return fail_line(
            make_error(DiagnosticCode::limit_line_bytes,
                       make_span(file, line_number, line_offset, 0, 1, line.size(), end_column),
                       "line is " + std::to_string(line.size()) + " bytes; limit is " +
                           std::to_string(limits.max_line_bytes) + " bytes"));
    }

    LineResult result;
    std::size_t offset = 0;
    std::uint32_t column = 1;

    const auto add_token = [&](TokenKind kind, std::string text, std::size_t begin_byte,
                               std::uint32_t begin_column, std::size_t end_byte,
                               std::uint32_t end_column) {
        result.tokens.push_back({
            kind,
            std::move(text),
            make_span(file, line_number, line_offset, begin_byte, begin_column, end_byte,
                      end_column),
        });
    };

    while (offset < line.size()) {
        if (is_horizontal_space(line[offset])) {
            ++offset;
            ++column;
            continue;
        }
        if (line[offset] == '#') {
            break;
        }

        const auto current = static_cast<unsigned char>(line[offset]);
        if (is_invalid_ascii_control(current)) {
            return fail_line(make_error(
                DiagnosticCode::dsl_invalid_character,
                make_span(file, line_number, line_offset, offset, column, offset + 1, column + 1),
                "control character is not allowed in the DSL"));
        }

        const auto begin_byte = offset;
        const auto begin_column = column;

        if (line[offset] == '=') {
            ++offset;
            ++column;
            add_token(TokenKind::equals, "=", begin_byte, begin_column, offset, column);
            continue;
        }

        if (starts_operator(line, offset)) {
            const auto kind =
                line[offset + 1] == '>' ? TokenKind::directed_arrow : TokenKind::bidirectional_link;
            const std::string text{line.substr(offset, 2)};
            offset += 2;
            column += 2;
            add_token(kind, text, begin_byte, begin_column, offset, column);
            continue;
        }

        if (line[offset] == '"') {
            ++offset;
            ++column;
            const auto text_begin = offset;
            while (offset < line.size() && line[offset] != '"') {
                const auto value = static_cast<unsigned char>(line[offset]);
                if (is_invalid_ascii_control(value)) {
                    return fail_line(
                        make_error(DiagnosticCode::dsl_invalid_character,
                                   make_span(file, line_number, line_offset, offset, column,
                                             offset + 1, column + 1),
                                   "control character is not allowed in a quoted string"));
                }
                const auto sequence = inspect_utf8(line, offset);
                offset += sequence.length;
                ++column;
            }
            if (offset == line.size()) {
                return fail_line(
                    make_error(DiagnosticCode::dsl_unterminated_string,
                               make_span(file, line_number, line_offset, begin_byte, begin_column,
                                         offset, column),
                               "quoted string is not terminated before the end of the line"));
            }

            const std::string text{line.substr(text_begin, offset - text_begin)};
            ++offset;
            ++column;
            add_token(TokenKind::quoted_string, text, begin_byte, begin_column, offset, column);

            if (offset < line.size() && !is_horizontal_space(line[offset]) && line[offset] != '#' &&
                line[offset] != '=' && !starts_operator(line, offset)) {
                return fail_line(make_error(DiagnosticCode::dsl_unexpected_token,
                                            make_span(file, line_number, line_offset, offset,
                                                      column, offset + 1, column + 1),
                                            "quoted string must be separated from the next token"));
            }
            continue;
        }

        while (offset < line.size() && !is_horizontal_space(line[offset]) && line[offset] != '#' &&
               line[offset] != '=' && line[offset] != '"' && !starts_operator(line, offset)) {
            const auto value = static_cast<unsigned char>(line[offset]);
            if (is_invalid_ascii_control(value)) {
                return fail_line(make_error(DiagnosticCode::dsl_invalid_character,
                                            make_span(file, line_number, line_offset, offset,
                                                      column, offset + 1, column + 1),
                                            "control character is not allowed in a bare token"));
            }
            const auto sequence = inspect_utf8(line, offset);
            offset += sequence.length;
            ++column;
        }

        if (offset == begin_byte || (offset < line.size() && line[offset] == '"')) {
            return fail_line(make_error(
                DiagnosticCode::dsl_unexpected_token,
                make_span(file, line_number, line_offset, offset, column, offset + 1, column + 1),
                "unexpected quote inside a bare token"));
        }

        add_token(TokenKind::bare, std::string{line.substr(begin_byte, offset - begin_byte)},
                  begin_byte, begin_column, offset, column);
    }

    return result;
}

} // namespace

std::string_view to_string(TokenKind value) noexcept {
    switch (value) {
    case TokenKind::bare:
        return "bare";
    case TokenKind::quoted_string:
        return "quoted-string";
    case TokenKind::equals:
        return "equals";
    case TokenKind::directed_arrow:
        return "directed-arrow";
    case TokenKind::bidirectional_link:
        return "bidirectional-link";
    }
    return "unknown";
}

LexResult lex_document(std::string_view source, StringId file, const ModelLimits &limits) {
    if (source.size() > limits.max_source_file_bytes) {
        return {
            {},
            make_error(DiagnosticCode::limit_source_file_bytes,
                       {
                           file,
                           {0, 1, 1},
                           {0, 1, 1},
                       },
                       "source file is " + std::to_string(source.size()) + " bytes; limit is " +
                           std::to_string(limits.max_source_file_bytes) + " bytes"),
        };
    }

    constexpr std::string_view kUtf8Bom{"\xEF\xBB\xBF", 3};
    if (source.substr(0, kUtf8Bom.size()) == kUtf8Bom) {
        return {
            {},
            make_error(DiagnosticCode::dsl_bom_not_allowed,
                       {
                           file,
                           {0, 1, 1},
                           {3, 1, 2},
                       },
                       "UTF-8 BOM is not allowed"),
        };
    }

    LexResult result;
    std::size_t line_offset = 0;
    std::uint32_t line_number = 1;
    while (line_offset < source.size()) {
        const auto newline = source.find('\n', line_offset);
        const auto raw_end = newline == std::string_view::npos ? source.size() : newline;
        auto content_end = raw_end;
        if (content_end > line_offset && source[content_end - 1] == '\r') {
            --content_end;
        }

        const auto line = source.substr(line_offset, content_end - line_offset);
        auto line_result =
            lex_line(line, file, line_number, static_cast<std::uint64_t>(line_offset), limits);
        if (line_result.error.has_value()) {
            return {{}, std::move(line_result.error)};
        }
        result.lines.push_back({line_number, std::move(line_result.tokens)});

        if (newline == std::string_view::npos) {
            break;
        }
        line_offset = newline + 1;
        ++line_number;
    }
    return result;
}

} // namespace ltfa::dsl
