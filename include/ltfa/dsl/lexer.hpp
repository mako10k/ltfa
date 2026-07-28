#pragma once

#include "ltfa/error.hpp"
#include "ltfa/limits.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ltfa::dsl {

enum class TokenKind {
    bare,
    quoted_string,
    equals,
    directed_arrow,
    bidirectional_link,
};

[[nodiscard]] std::string_view to_string(TokenKind value) noexcept;

struct Token {
    TokenKind kind{TokenKind::bare};
    std::string text;
    SourceSpan source;
};

struct LexedLine {
    std::uint32_t line{1};
    std::vector<Token> tokens;
};

struct LexResult {
    std::vector<LexedLine> lines;
    std::optional<Diagnostic> error;

    [[nodiscard]] bool ok() const noexcept { return !error.has_value(); }
};

[[nodiscard]] LexResult lex_document(std::string_view source, StringId file,
                                     const ModelLimits &limits = kDefaultModelLimits);

} // namespace ltfa::dsl
