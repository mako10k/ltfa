#include "ltfa/dsl/parser.hpp"
#include "ltfa/model_compiler.hpp"
#include "ltfa/model_output.hpp"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>

namespace {

constexpr ltfa::StringId kFuzzFile{1};
constexpr std::size_t kStandaloneInputLimit = 1024U * 1024U;

void fuzz_one_input(std::string_view input) {
    auto limits = ltfa::kDefaultModelLimits;
    limits.max_source_file_bytes = kStandaloneInputLimit;
    limits.max_total_source_bytes = kStandaloneInputLimit;
    limits.max_line_bytes = 64U * 1024U;
    limits.max_include_depth = 4;
    limits.max_entities = 4'096;
    limits.max_relations = 8'192;
    limits.max_attributes_per_object = 64;
    limits.max_attribute_string_bytes = 8U * 1024U;

    auto parsed = ltfa::dsl::parse_document(input, kFuzzFile, limits);
    if (!parsed.ok()) {
        return;
    }

    ltfa::dsl::LoadedDocument loaded;
    loaded.statements = std::move(parsed.document->statements);
    loaded.total_source_bytes = input.size();
    auto compiled = ltfa::compile_model(loaded, limits);
    if (!compiled.ok()) {
        return;
    }

    const auto json = ltfa::render_model_json(*compiled.model);
    const auto dot = ltfa::render_model_dot(*compiled.model);
    volatile std::size_t rendered_bytes = json.size() + dot.size();
    (void)rendered_bytes;
}

} // namespace

#ifdef LTFA_LIBFUZZER

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size) {
    const auto *bytes = data == nullptr ? "" : reinterpret_cast<const char *>(data);
    fuzz_one_input({bytes, size});
    return 0;
}

#else

int main(int argc, char **argv) {
    if (argc == 1) {
        std::string input{std::istreambuf_iterator<char>{std::cin},
                          std::istreambuf_iterator<char>{}};
        if (input.size() > kStandaloneInputLimit) {
            std::cerr << "stdin exceeds standalone fuzz input limit\n";
            return 2;
        }
        fuzz_one_input(input);
        return 0;
    }

    for (int index = 1; index < argc; ++index) {
        std::ifstream input{argv[index], std::ios::binary};
        if (!input) {
            std::cerr << "cannot read fuzz seed: " << argv[index] << '\n';
            return 2;
        }
        std::string bytes{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
        if (bytes.size() > kStandaloneInputLimit) {
            std::cerr << "fuzz seed exceeds standalone input limit: " << argv[index] << '\n';
            return 2;
        }
        fuzz_one_input(bytes);
    }
    return 0;
}

#endif
