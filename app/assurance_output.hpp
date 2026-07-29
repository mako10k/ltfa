#pragma once

#include "ltfa/failure_propagation.hpp"
#include "ltfa/path_analysis.hpp"
#include "ltfa/spof_analysis.hpp"

#include <optional>
#include <string>
#include <vector>

namespace ltfa::app {

struct AssuranceBudgets {
    TraversalBudget path;
    SpofBudget spof;
    PropagationBudget propagation;
};

struct AssuranceReport {
    bool incomplete{false};
    ScopeId scope;
    ObjectRef source;
    ObjectRef target;
    bool include_restricted{false};
    std::vector<std::string> model_inputs;
    PathResult path;
    std::optional<SpofAnalysisResult> spof;
    std::optional<PropagationResult> propagation;
    std::optional<std::string> failure_external_id;
    AssuranceBudgets budgets;
};

[[nodiscard]] std::string render_assurance_human(const NormalizedModel &model,
                                                 const AssuranceReport &report);
[[nodiscard]] std::string render_assurance_json(const NormalizedModel &model,
                                                const AssuranceReport &report);

} // namespace ltfa::app
