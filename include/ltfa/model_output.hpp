#pragma once

#include "ltfa/model.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ltfa {

enum class RestrictedOutputPolicy {
    omit,
    include_explicit,
};

enum class DotGrouping {
    none,
    scope,
    plane,
    layer,
    failure_domain,
};

enum class DotView {
    raw,
    topology,
    power,
    dependency,
    overlay,
};

[[nodiscard]] std::string_view to_string(DotView value) noexcept;

struct ModelOutputContext {
    std::vector<std::string> model_inputs;
    std::vector<std::string> observation_inputs;
    std::optional<std::string> analysis_at;
    RestrictedOutputPolicy restricted_policy{RestrictedOutputPolicy::omit};
};

struct DotOutputOptions {
    DotView view{DotView::raw};
    DotGrouping grouping{DotGrouping::scope};
    std::optional<Plane> plane;
    std::optional<Layer> layer;
    std::optional<FailureDomainId> failure_domain;
    std::vector<ObjectRef> focus_objects;
    std::vector<ObjectRef> highlighted_objects;
    std::vector<RelationId> highlighted_relations;
};

[[nodiscard]] std::string render_model_human(const NormalizedModel &model,
                                             const ModelOutputContext &context = {});
[[nodiscard]] std::string render_model_json(const NormalizedModel &model,
                                            const ModelOutputContext &context = {});
[[nodiscard]] std::string render_model_dot(const NormalizedModel &model,
                                           const ModelOutputContext &context = {},
                                           const DotOutputOptions &options = {});

} // namespace ltfa
