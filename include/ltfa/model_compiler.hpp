#pragma once

#include "ltfa/dsl/include_resolver.hpp"
#include "ltfa/model.hpp"

#include <optional>

namespace ltfa {

struct CompileResult {
    std::optional<NormalizedModel> model;
    std::optional<Diagnostic> error;

    [[nodiscard]] bool ok() const noexcept { return model.has_value() && !error.has_value(); }
};

[[nodiscard]] CompileResult compile_model(const dsl::LoadedDocument &document,
                                          const ModelLimits &limits = kDefaultModelLimits);

} // namespace ltfa
