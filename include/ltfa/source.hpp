#pragma once

#include "ltfa/ids.hpp"

#include <cstdint>

namespace ltfa {

struct SourcePosition {
    std::uint64_t byte_offset{0};
    std::uint32_t line{1};
    std::uint32_t column{1};
};

struct SourceSpan {
    StringId file;
    SourcePosition begin;
    SourcePosition end;

    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return file.is_valid() && begin.line >= 1 && begin.column >= 1 && end.line >= 1 &&
               end.column >= 1 && begin.byte_offset <= end.byte_offset;
    }
};

} // namespace ltfa
