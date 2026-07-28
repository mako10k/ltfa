#include "ltfa/version.hpp"

#include "ltfa/version_config.hpp"

namespace ltfa {

const Version kVersion{LTFA_VERSION_MAJOR, LTFA_VERSION_MINOR, LTFA_VERSION_PATCH};

std::string_view project_name() noexcept { return LTFA_PROJECT_NAME; }

std::string_view version_string() noexcept { return LTFA_VERSION_STRING; }

} // namespace ltfa
