#pragma once

#include <iosfwd>

namespace ltfa::app {

[[nodiscard]] int run_cli(int argc, char *argv[], std::ostream &output, std::ostream &error);

} // namespace ltfa::app
