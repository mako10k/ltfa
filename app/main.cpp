#include "cli.hpp"

#include <iostream>

int main(int argc, char *argv[]) { return ltfa::app::run_cli(argc, argv, std::cout, std::cerr); }
