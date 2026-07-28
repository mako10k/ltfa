#include "ltfa/dsl/lexer.hpp"

#include <array>
#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

int failures = 0;
constexpr ltfa::StringId kFile{1};

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << message << '\n';
        ++failures;
    }
}

const ltfa::Diagnostic *expect_error(const ltfa::dsl::LexResult &result,
                                     ltfa::DiagnosticCode expected) {
    if (!result.error.has_value()) {
        std::cerr << "expected diagnostic " << ltfa::to_string(expected) << '\n';
        ++failures;
        return nullptr;
    }
    if (result.error->code != expected) {
        std::cerr << "expected diagnostic " << ltfa::to_string(expected) << ", got "
                  << ltfa::to_string(result.error->code) << '\n';
        ++failures;
    }
    expect(result.lines.empty(), "failed lexing must not expose partial lines");
    return &*result.error;
}

void expect_tokens(const ltfa::dsl::LexedLine &line,
                   const std::vector<std::pair<ltfa::dsl::TokenKind, std::string_view>> &expected) {
    if (line.tokens.size() != expected.size()) {
        std::cerr << "expected " << expected.size() << " tokens, got " << line.tokens.size()
                  << '\n';
        ++failures;
        return;
    }
    for (std::size_t index = 0; index < expected.size(); ++index) {
        if (line.tokens[index].kind != expected[index].first ||
            line.tokens[index].text != expected[index].second) {
            std::cerr << "unexpected token " << index
                      << ": kind=" << ltfa::dsl::to_string(line.tokens[index].kind)
                      << " text=" << line.tokens[index].text << '\n';
            ++failures;
        }
    }
}

std::string_view source_lexeme(std::string_view source, const ltfa::dsl::Token &token) {
    const auto begin = static_cast<std::size_t>(token.source.begin.byte_offset);
    const auto length =
        static_cast<std::size_t>(token.source.end.byte_offset - token.source.begin.byte_offset);
    return source.substr(begin, length);
}

void test_token_contract() {
    const std::array cases{
        std::pair{ltfa::dsl::TokenKind::bare, std::string_view{"bare"}},
        std::pair{ltfa::dsl::TokenKind::quoted_string, std::string_view{"quoted-string"}},
        std::pair{ltfa::dsl::TokenKind::equals, std::string_view{"equals"}},
        std::pair{ltfa::dsl::TokenKind::directed_arrow, std::string_view{"directed-arrow"}},
        std::pair{ltfa::dsl::TokenKind::bidirectional_link, std::string_view{"bidirectional-link"}},
    };
    for (const auto &[value, expected] : cases) {
        expect(ltfa::dsl::to_string(value) == expected, "token name must remain stable");
    }
}

void test_basic_tokens_comments_and_utf8() {
    const std::string source = "scope \"site:ami-a\" kind=site label=\"阿見 A\" # operator note\n";
    const auto result = ltfa::dsl::lex_document(source, kFile);
    expect(result.ok(), "valid UTF-8 DSL line must lex");
    expect(result.lines.size() == 1, "one physical line must produce one lexed line");
    if (!result.ok() || result.lines.size() != 1) {
        return;
    }

    expect_tokens(result.lines[0], {
                                       {ltfa::dsl::TokenKind::bare, "scope"},
                                       {ltfa::dsl::TokenKind::quoted_string, "site:ami-a"},
                                       {ltfa::dsl::TokenKind::bare, "kind"},
                                       {ltfa::dsl::TokenKind::equals, "="},
                                       {ltfa::dsl::TokenKind::bare, "site"},
                                       {ltfa::dsl::TokenKind::bare, "label"},
                                       {ltfa::dsl::TokenKind::equals, "="},
                                       {ltfa::dsl::TokenKind::quoted_string, "阿見 A"},
                                   });

    const auto &scope = result.lines[0].tokens[0];
    expect(scope.source.file == kFile, "token span must retain the caller's source file ID");
    expect(scope.source.begin.line == 1 && scope.source.begin.column == 1,
           "first token must start at line 1 column 1");
    expect(scope.source.end.byte_offset == 5 && scope.source.end.column == 6,
           "ASCII token span must be half-open");
    expect(source_lexeme(source, result.lines[0].tokens[1]) == "\"site:ami-a\"",
           "quoted token span must include its delimiters");
    expect(source_lexeme(source, result.lines[0].tokens.back()) == "\"阿見 A\"",
           "UTF-8 token span must preserve source bytes");
}

void test_unicode_columns_and_crlf_offsets() {
    const std::string source = "label=\"阿見\" kind=site\r\nscope x\r\n";
    const auto result = ltfa::dsl::lex_document(source, kFile);
    expect(result.ok(), "CRLF source with UTF-8 labels must lex");
    expect(result.lines.size() == 2, "CRLF source must retain physical line count");
    if (!result.ok() || result.lines.size() != 2) {
        return;
    }

    const auto &kind = result.lines[0].tokens[3];
    expect(kind.text == "kind", "kind token must be present after UTF-8 string");
    expect(kind.source.begin.byte_offset == 15, "UTF-8 span must use byte offsets");
    expect(kind.source.begin.column == 12, "UTF-8 span column must count code points");

    const auto &scope = result.lines[1].tokens[0];
    expect(scope.source.begin.byte_offset == 26, "line 2 span must include CRLF bytes");
    expect(scope.source.begin.line == 2 && scope.source.begin.column == 1,
           "line 2 token must have source-faithful coordinates");
}

void test_operators_hyphens_and_opaque_text() {
    const std::string source =
        "connect a--b kind=power-input\n"
        "relate a->b valid-for=60s\n"
        "entity \"$(cmd)\" value=\"${HOME}\" raw=`date` note=\"a#b\" # ignored\n";
    const auto result = ltfa::dsl::lex_document(source, kFile);
    expect(result.ok(), "operators and opaque input text must lex");
    expect(result.lines.size() == 3, "three statements must remain separate lines");
    if (!result.ok() || result.lines.size() != 3) {
        return;
    }

    expect_tokens(result.lines[0], {
                                       {ltfa::dsl::TokenKind::bare, "connect"},
                                       {ltfa::dsl::TokenKind::bare, "a"},
                                       {ltfa::dsl::TokenKind::bidirectional_link, "--"},
                                       {ltfa::dsl::TokenKind::bare, "b"},
                                       {ltfa::dsl::TokenKind::bare, "kind"},
                                       {ltfa::dsl::TokenKind::equals, "="},
                                       {ltfa::dsl::TokenKind::bare, "power-input"},
                                   });
    expect_tokens(result.lines[1], {
                                       {ltfa::dsl::TokenKind::bare, "relate"},
                                       {ltfa::dsl::TokenKind::bare, "a"},
                                       {ltfa::dsl::TokenKind::directed_arrow, "->"},
                                       {ltfa::dsl::TokenKind::bare, "b"},
                                       {ltfa::dsl::TokenKind::bare, "valid-for"},
                                       {ltfa::dsl::TokenKind::equals, "="},
                                       {ltfa::dsl::TokenKind::bare, "60s"},
                                   });
    expect_tokens(result.lines[2], {
                                       {ltfa::dsl::TokenKind::bare, "entity"},
                                       {ltfa::dsl::TokenKind::quoted_string, "$(cmd)"},
                                       {ltfa::dsl::TokenKind::bare, "value"},
                                       {ltfa::dsl::TokenKind::equals, "="},
                                       {ltfa::dsl::TokenKind::quoted_string, "${HOME}"},
                                       {ltfa::dsl::TokenKind::bare, "raw"},
                                       {ltfa::dsl::TokenKind::equals, "="},
                                       {ltfa::dsl::TokenKind::bare, "`date`"},
                                       {ltfa::dsl::TokenKind::bare, "note"},
                                       {ltfa::dsl::TokenKind::equals, "="},
                                       {ltfa::dsl::TokenKind::quoted_string, "a#b"},
                                   });
}

void test_limits() {
    auto limits = ltfa::kDefaultModelLimits;
    limits.max_source_file_bytes = 4;
    auto result = ltfa::dsl::lex_document("12345", kFile, limits);
    const auto *file_error = expect_error(result, ltfa::DiagnosticCode::limit_source_file_bytes);
    if (file_error != nullptr) {
        expect(file_error->message.find("5 bytes") != std::string::npos,
               "file-limit diagnostic must report actual size");
        expect(file_error->message.find("4 bytes") != std::string::npos,
               "file-limit diagnostic must report configured limit");
        expect(file_error->source.begin.line == 1 && file_error->source.begin.column == 1 &&
                   file_error->source.end.byte_offset == 0,
               "file-limit diagnostic must use an honest file-start location");
    }

    limits = ltfa::kDefaultModelLimits;
    limits.max_line_bytes = 4;
    result = ltfa::dsl::lex_document("阿ab\nok", kFile, limits);
    const auto *line_error = expect_error(result, ltfa::DiagnosticCode::limit_line_bytes);
    if (line_error != nullptr) {
        expect(line_error->source.begin.line == 1,
               "line-limit diagnostic must identify the offending line");
        expect(line_error->source.end.byte_offset == 5 && line_error->source.end.column == 4,
               "line-limit diagnostic must retain byte and Unicode-column coordinates");
    }
}

void test_malformed_text() {
    auto result = ltfa::dsl::lex_document("\xEF\xBB\xBFltfa 1", kFile);
    const auto *bom_error = expect_error(result, ltfa::DiagnosticCode::dsl_bom_not_allowed);
    if (bom_error != nullptr) {
        expect(bom_error->source.begin.byte_offset == 0 && bom_error->source.end.byte_offset == 3,
               "BOM diagnostic must cover the BOM bytes");
    }

    result = ltfa::dsl::lex_document("scope \"unterminated", kFile);
    const auto *quote_error = expect_error(result, ltfa::DiagnosticCode::dsl_unterminated_string);
    if (quote_error != nullptr) {
        expect(quote_error->source.begin.column == 7,
               "unterminated-string diagnostic must begin at the opening quote");
    }

    result = ltfa::dsl::lex_document("scope \"x\"kind=site", kFile);
    expect_error(result, ltfa::DiagnosticCode::dsl_unexpected_token);

    std::string control{"entity x"};
    control.push_back('\0');
    control.append(" kind=device");
    result = ltfa::dsl::lex_document(control, kFile);
    const auto *control_error = expect_error(result, ltfa::DiagnosticCode::dsl_invalid_character);
    if (control_error != nullptr) {
        expect(control_error->source.begin.byte_offset == 8,
               "control-character diagnostic must identify the exact byte");
    }
}

void test_utf8_rejection_boundaries() {
    const std::array invalid_sequences{
        std::string{"\"\xC0\xAF\"", 4},
        std::string{"\"\xED\xA0\x80\"", 5},
        std::string{"\"\xF4\x90\x80\x80\"", 6},
        std::string{"\"\xE3\x81\"", 4},
    };
    for (const auto &source : invalid_sequences) {
        const auto result = ltfa::dsl::lex_document(source, kFile);
        expect_error(result, ltfa::DiagnosticCode::dsl_invalid_utf8);
    }

    std::string second_line{"ok\n\""};
    second_line.push_back(static_cast<char>(0xC0));
    second_line.push_back('"');
    const auto second_line_result = ltfa::dsl::lex_document(second_line, kFile);
    const auto *second_line_error =
        expect_error(second_line_result, ltfa::DiagnosticCode::dsl_invalid_utf8);
    if (second_line_error != nullptr) {
        expect(second_line_error->source.begin.byte_offset == 4 &&
                   second_line_error->source.begin.line == 2 &&
                   second_line_error->source.begin.column == 2,
               "UTF-8 diagnostic must retain exact file, line, column, and byte position");
    }

    const std::string highest_scalar{"\"\xF4\x8F\xBF\xBF\"", 6};
    const auto result = ltfa::dsl::lex_document(highest_scalar, kFile);
    expect(result.ok(), "highest valid Unicode scalar must be accepted");
    if (result.ok()) {
        expect(result.lines[0].tokens[0].text.size() == 4,
               "valid four-byte scalar must be retained byte-for-byte");
    }
}

} // namespace

int main() {
    test_token_contract();
    test_basic_tokens_comments_and_utf8();
    test_unicode_columns_and_crlf_offsets();
    test_operators_hyphens_and_opaque_text();
    test_limits();
    test_malformed_text();
    test_utf8_rejection_boundaries();
    return failures == 0 ? 0 : 1;
}
