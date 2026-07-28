#pragma once

#include "ltfa/dsl/parser.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace ltfa::dsl {

struct LoadedSource {
    StringId id;
    std::filesystem::path canonical_path;
    std::uint64_t byte_size{0};
    std::optional<SourceSpan> header_source;
};

struct LoadedDocument {
    std::filesystem::path canonical_model_root;
    std::vector<Statement> statements;
    std::uint64_t total_source_bytes{0};
};

struct LoadResult {
    std::vector<LoadedSource> sources;
    std::optional<LoadedDocument> document;
    std::optional<Diagnostic> error;

    [[nodiscard]] bool ok() const noexcept { return document.has_value() && !error.has_value(); }
};

[[nodiscard]] LoadResult load_document_tree(const std::filesystem::path &model_root,
                                            const std::filesystem::path &entry_file,
                                            const ModelLimits &limits = kDefaultModelLimits);

} // namespace ltfa::dsl
