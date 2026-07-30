#include "ltfa/version.hpp"

#include <iostream>

int main() {
    if (ltfa::project_name() != "ltfa") {
        std::cerr << "unexpected project name\n";
        return 1;
    }

    if (ltfa::version_string() != "0.2.1") {
        std::cerr << "unexpected version string\n";
        return 1;
    }

    if (ltfa::kVersion.major != 0 || ltfa::kVersion.minor != 2 || ltfa::kVersion.patch != 1) {
        std::cerr << "unexpected structured version\n";
        return 1;
    }

    return 0;
}
