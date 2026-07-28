#include "ltfa/version.hpp"

#include <iostream>
#include <string_view>

namespace {

void print_usage(std::ostream &output) {
    output << "LTFA - lightweight multilayer topology and fault analyzer\n"
           << "\n"
           << "Usage:\n"
           << "  ltfa --help\n"
           << "  ltfa --version\n"
           << "\n"
           << "Status:\n"
           << "  Bootstrap scaffold only; topology analysis is not implemented.\n";
}

} // namespace

int main(int argc, char *argv[]) {
    if (argc == 1) {
        print_usage(std::cout);
        return 0;
    }

    if (argc == 2) {
        const std::string_view argument{argv[1]};
        if (argument == "--help" || argument == "-h") {
            print_usage(std::cout);
            return 0;
        }
        if (argument == "--version") {
            std::cout << ltfa::project_name() << ' ' << ltfa::version_string() << '\n';
            return 0;
        }
    }

    std::cerr << "ltfa: unsupported arguments; use --help\n";
    return 2;
}
