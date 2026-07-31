#include "ltfa/observation.hpp"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>

namespace {

constexpr ltfa::StringId kFuzzFile{2};
constexpr std::size_t kStandaloneInputLimit = 1024U * 1024U;

void fuzz_one_input(std::string_view input) {
    auto limits = ltfa::kDefaultModelLimits;
    limits.max_observation_input_bytes = kStandaloneInputLimit;
    limits.max_observation_line_bytes = 64U * 1024U;
    limits.max_observations = 4'096;
    limits.max_observation_extensions = 16;
    limits.max_observation_string_bytes = 8U * 1024U;

    const auto decoded = ltfa::decode_observation_json_lines(input, kFuzzFile, limits);
    if (!decoded.ok()) {
        return;
    }

    std::size_t retained_bytes = 0;
    for (const auto &observation : decoded.observations) {
        retained_bytes += observation.schema.size() + observation.probe_id.size() +
                          observation.target.size() + observation.vantage.size() +
                          observation.observed_at.text.size() + observation.extensions.size();
    }
    volatile std::size_t observed_bytes = retained_bytes;
    (void)observed_bytes;
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
