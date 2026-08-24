#include "ltfa/dsl/include_resolver.hpp"
#include "ltfa/error.hpp"
#include "ltfa/evidence.hpp"
#include "ltfa/limits.hpp"
#include "ltfa/model_compiler.hpp"
#include "ltfa/observation.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace {

using Clock = std::chrono::steady_clock;

void print_usage(std::string_view program) {
    std::cerr << "usage:\n"
              << "  " << program << " model <model-root> <entry-file>\n"
              << "  " << program
              << " observations <model-root> <entry-file> <observations-file> <analysis-at>\n";
}

void print_diagnostic(const ltfa::Diagnostic &diagnostic) {
    std::cerr << ltfa::to_string(diagnostic.code) << ": " << diagnostic.message << '\n';
}

std::optional<ltfa::NormalizedModel> compile_model(const std::filesystem::path &model_root,
                                                   const std::filesystem::path &entry_file) {
    auto loaded = ltfa::dsl::load_document_tree(model_root, entry_file);
    if (!loaded.ok()) {
        if (loaded.error.has_value()) {
            print_diagnostic(*loaded.error);
        }
        return std::nullopt;
    }
    auto compiled = ltfa::compile_model(*loaded.document);
    if (!compiled.ok()) {
        if (compiled.error.has_value()) {
            print_diagnostic(*compiled.error);
        }
        return std::nullopt;
    }
    return std::move(compiled.model);
}

std::optional<std::string> read_observations(const std::filesystem::path &path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input.is_open()) {
        std::cerr << "cannot open observation input: " << path << '\n';
        return std::nullopt;
    }
    const auto end = input.tellg();
    if (end < 0) {
        std::cerr << "cannot determine observation input size: " << path << '\n';
        return std::nullopt;
    }
    const auto size = static_cast<std::uint64_t>(end);
    if (size > ltfa::kDefaultModelLimits.max_observation_input_bytes) {
        std::cerr << "observation input exceeds the configured byte limit\n";
        return std::nullopt;
    }
    std::string payload(static_cast<std::size_t>(size), '\0');
    input.seekg(0);
    if (!payload.empty() &&
        !input.read(payload.data(), static_cast<std::streamsize>(payload.size()))) {
        std::cerr << "cannot read observation input: " << path << '\n';
        return std::nullopt;
    }
    return payload;
}

std::int64_t elapsed_microseconds(Clock::time_point start, Clock::time_point finish) {
    return std::chrono::duration_cast<std::chrono::microseconds>(finish - start).count();
}

int benchmark_model(const std::filesystem::path &model_root,
                    const std::filesystem::path &entry_file) {
    const auto start = Clock::now();
    auto model = compile_model(model_root, entry_file);
    const auto finish = Clock::now();
    if (!model.has_value()) {
        return 1;
    }
    std::cout << "phase=model" << " elapsed_us=" << elapsed_microseconds(start, finish)
              << " entities=" << model->entities.size() << " relations=" << model->relations.size()
              << " probes=" << model->probes.size() << '\n';
    return 0;
}

int benchmark_observations(const std::filesystem::path &model_root,
                           const std::filesystem::path &entry_file,
                           const std::filesystem::path &observation_file,
                           std::string_view analysis_at_text) {
    auto model = compile_model(model_root, entry_file);
    if (!model.has_value()) {
        return 1;
    }
    auto analysis_at = ltfa::parse_observation_timestamp(analysis_at_text);
    if (!analysis_at.ok()) {
        if (analysis_at.error.has_value()) {
            print_diagnostic(*analysis_at.error);
        }
        return 1;
    }

    const auto start = Clock::now();
    auto payload = read_observations(model_root / observation_file);
    if (!payload.has_value()) {
        return 1;
    }
    auto decoded = ltfa::decode_observation_json_lines(*payload, ltfa::StringId{0});
    if (!decoded.ok()) {
        if (decoded.error.has_value()) {
            print_diagnostic(*decoded.error);
        }
        return 1;
    }
    auto evidence = ltfa::build_evidence_index(*model, std::move(decoded.observations),
                                               std::move(*analysis_at.timestamp));
    const auto finish = Clock::now();
    if (!evidence.ok()) {
        if (evidence.error.has_value()) {
            print_diagnostic(*evidence.error);
        }
        return 1;
    }

    std::size_t current = 0;
    for (const auto &observation : evidence.index->observations) {
        if (observation.freshness == ltfa::EvidenceFreshness::current) {
            ++current;
        }
    }
    std::cout << "phase=observations" << " elapsed_us=" << elapsed_microseconds(start, finish)
              << " observations=" << evidence.index->observations.size() << " current=" << current
              << " stale=" << evidence.index->observations.size() - current
              << " groups=" << evidence.index->groups.size()
              << " conflicts=" << evidence.index->conflicts.size() << '\n';
    return 0;
}

} // namespace

int main(int argc, char **argv) {
    if (argc == 4 && std::string_view{argv[1]} == "model") {
        return benchmark_model(argv[2], argv[3]);
    }
    if (argc == 6 && std::string_view{argv[1]} == "observations") {
        return benchmark_observations(argv[2], argv[3], argv[4], argv[5]);
    }
    print_usage(argv[0]);
    return 2;
}
