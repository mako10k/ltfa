#pragma once

#include <string_view>

namespace ltfa {

struct Version {
    int major;
    int minor;
    int patch;
};

extern const Version kVersion;

[[nodiscard]] std::string_view project_name() noexcept;
[[nodiscard]] std::string_view version_string() noexcept;

} // namespace ltfa
