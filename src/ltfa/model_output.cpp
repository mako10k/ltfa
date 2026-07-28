#include "ltfa/model_output.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ltfa {
namespace {

constexpr std::string_view kModelSchema{"ltfa.model/v1"};
constexpr std::string_view kDotSchema{"ltfa.model.dot/v1"};

struct Visibility {
    std::vector<bool> entities;
    std::vector<bool> endpoints;
    std::vector<bool> relations;
    std::vector<bool> requirement_groups;
    std::vector<bool> failure_domains;
    std::vector<bool> failure_modes;
    std::vector<bool> probes;
    std::size_t restricted_omissions{0};
    std::size_t evidence_omissions{0};
};

struct DotSelection {
    std::vector<bool> entities;
    std::vector<bool> endpoints;
    std::vector<bool> relations;
};

[[nodiscard]] bool valid_object(const NormalizedModel &model, ObjectRef object) noexcept {
    if (!object.is_valid()) {
        return false;
    }
    return object.kind == ObjectKind::entity ? object.index < model.entities.size()
                                             : object.index < model.endpoints.size();
}

[[nodiscard]] bool object_visible(const Visibility &visibility, ObjectRef object) noexcept {
    if (!object.is_valid()) {
        return false;
    }
    if (object.kind == ObjectKind::entity) {
        return object.index < visibility.entities.size() && visibility.entities[object.index];
    }
    return object.index < visibility.endpoints.size() && visibility.endpoints[object.index];
}

[[nodiscard]] std::vector<std::string>
sorted_unique_strings(const std::vector<std::string> &values) {
    auto result = values;
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

template <typename Value>
[[nodiscard]] std::vector<std::size_t> sorted_indices(const NormalizedModel &model,
                                                      const std::vector<Value> &values) {
    std::vector<std::size_t> indices(values.size());
    for (std::size_t index = 0; index < values.size(); ++index) {
        indices[index] = index;
    }
    std::sort(indices.begin(), indices.end(), [&](std::size_t left, std::size_t right) {
        return model.string(values[left].external_id) < model.string(values[right].external_id);
    });
    return indices;
}

template <typename Id>
[[nodiscard]] bool range_contains(const std::vector<Id> &values, IndexRange range, Id target) {
    const auto begin = std::min<std::size_t>(range.begin, values.size());
    const auto end = std::min<std::size_t>(begin + range.count, values.size());
    return std::find(values.begin() + static_cast<std::ptrdiff_t>(begin),
                     values.begin() + static_cast<std::ptrdiff_t>(end),
                     target) != values.begin() + static_cast<std::ptrdiff_t>(end);
}

[[nodiscard]] bool object_in_domain(const NormalizedModel &model, ObjectRef object,
                                    FailureDomainId domain) {
    if (!valid_object(model, object) || !domain.is_valid() ||
        domain.value() >= model.failure_domains.size()) {
        return false;
    }
    const auto range = object.kind == ObjectKind::entity
                           ? model.entities[object.index].failure_domains
                           : model.endpoints[object.index].failure_domains;
    return range_contains(model.object_failure_domains, range, domain);
}

[[nodiscard]] const ModelAttribute *find_attribute(const NormalizedModel &model, IndexRange range,
                                                   std::string_view key) {
    const auto begin = std::min<std::size_t>(range.begin, model.attributes.size());
    const auto end = std::min<std::size_t>(begin + range.count, model.attributes.size());
    for (auto index = begin; index < end; ++index) {
        if (model.string(model.attributes[index].key) == key) {
            return &model.attributes[index];
        }
    }
    return nullptr;
}

[[nodiscard]] bool relation_restricted(const NormalizedModel &model, const Relation &relation) {
    const auto *attribute = find_attribute(model, relation.attributes, "sensitivity");
    return attribute != nullptr && model.string(attribute->value) == "restricted";
}

[[nodiscard]] Visibility build_visibility(const NormalizedModel &model,
                                          const ModelOutputContext &context) {
    const bool include_restricted =
        context.restricted_policy == RestrictedOutputPolicy::include_explicit;
    Visibility visibility;
    visibility.entities.resize(model.entities.size());
    visibility.endpoints.resize(model.endpoints.size());
    visibility.relations.resize(model.relations.size());
    visibility.requirement_groups.resize(model.requirement_groups.size());
    visibility.failure_domains.resize(model.failure_domains.size());
    visibility.failure_modes.resize(model.failure_modes.size());
    visibility.probes.resize(model.probes.size());

    for (const auto &entity : model.entities) {
        visibility.entities[entity.id.value()] =
            include_restricted || entity.sensitivity != Sensitivity::restricted;
    }
    bool changed = true;
    while (!include_restricted && changed) {
        changed = false;
        for (const auto &entity : model.entities) {
            if (visibility.entities[entity.id.value()] && entity.network_space.is_valid() &&
                (entity.network_space.value() >= visibility.entities.size() ||
                 !visibility.entities[entity.network_space.value()])) {
                visibility.entities[entity.id.value()] = false;
                changed = true;
            }
        }
    }
    for (const auto &entity : model.entities) {
        if (!visibility.entities[entity.id.value()]) {
            ++visibility.restricted_omissions;
        } else if (!include_restricted && entity.source_ref.is_valid()) {
            ++visibility.evidence_omissions;
        }
    }
    for (const auto &endpoint : model.endpoints) {
        const bool owner_visible = endpoint.owner.is_valid() &&
                                   endpoint.owner.value() < visibility.entities.size() &&
                                   visibility.entities[endpoint.owner.value()];
        const bool network_space_visible =
            !endpoint.network_space.is_valid() ||
            (endpoint.network_space.value() < visibility.entities.size() &&
             visibility.entities[endpoint.network_space.value()]);
        visibility.endpoints[endpoint.id.value()] =
            owner_visible && network_space_visible &&
            (include_restricted || endpoint.sensitivity != Sensitivity::restricted);
        if (!visibility.endpoints[endpoint.id.value()]) {
            ++visibility.restricted_omissions;
        } else if (!include_restricted && endpoint.source_ref.is_valid()) {
            ++visibility.evidence_omissions;
        }
    }
    for (const auto &relation : model.relations) {
        visibility.relations[relation.id.value()] =
            object_visible(visibility, relation.from) && object_visible(visibility, relation.to) &&
            (include_restricted || !relation_restricted(model, relation));
        if (!visibility.relations[relation.id.value()]) {
            ++visibility.restricted_omissions;
        } else if (!include_restricted &&
                   find_attribute(model, relation.attributes, "source-ref") != nullptr) {
            ++visibility.evidence_omissions;
        }
    }
    for (const auto &requirement : model.requirement_groups) {
        bool visible = requirement.consumer.is_valid() &&
                       requirement.consumer.value() < visibility.entities.size() &&
                       visibility.entities[requirement.consumer.value()];
        const auto begin =
            std::min<std::size_t>(requirement.providers.begin, model.requirement_providers.size());
        const auto end = std::min<std::size_t>(begin + requirement.providers.count,
                                               model.requirement_providers.size());
        for (auto index = begin; visible && index < end; ++index) {
            const auto provider = model.requirement_providers[index];
            visible = provider.is_valid() && provider.value() < visibility.entities.size() &&
                      visibility.entities[provider.value()];
        }
        visibility.requirement_groups[requirement.id.value()] = visible;
        if (!visible) {
            ++visibility.restricted_omissions;
        }
    }
    for (const auto &domain : model.failure_domains) {
        const auto begin =
            std::min<std::size_t>(domain.members.begin, model.failure_domain_members.size());
        const auto end = std::min<std::size_t>(begin + domain.members.count,
                                               model.failure_domain_members.size());
        const bool visible =
            std::any_of(model.failure_domain_members.begin() + static_cast<std::ptrdiff_t>(begin),
                        model.failure_domain_members.begin() + static_cast<std::ptrdiff_t>(end),
                        [&](ObjectRef member) { return object_visible(visibility, member); });
        visibility.failure_domains[domain.id.value()] = visible;
        if (!visible && domain.members.count != 0) {
            ++visibility.restricted_omissions;
        }
    }
    for (const auto &failure : model.failure_modes) {
        visibility.failure_modes[failure.id.value()] = object_visible(visibility, failure.target);
        if (!visibility.failure_modes[failure.id.value()]) {
            ++visibility.restricted_omissions;
        }
    }
    for (const auto &probe : model.probes) {
        visibility.probes[probe.id.value()] =
            object_visible(visibility, probe.target) && object_visible(visibility, probe.vantage);
        if (!visibility.probes[probe.id.value()]) {
            ++visibility.restricted_omissions;
        }
    }
    return visibility;
}

[[nodiscard]] std::string escape_json(std::string_view value) {
    std::ostringstream output;
    output << '"';
    for (const unsigned char character : value) {
        switch (character) {
        case '"':
            output << "\\\"";
            break;
        case '\\':
            output << "\\\\";
            break;
        case '\b':
            output << "\\b";
            break;
        case '\f':
            output << "\\f";
            break;
        case '\n':
            output << "\\n";
            break;
        case '\r':
            output << "\\r";
            break;
        case '\t':
            output << "\\t";
            break;
        default:
            if (character < 0x20U) {
                output << "\\u00" << std::hex << std::setw(2) << std::setfill('0')
                       << static_cast<unsigned int>(character) << std::dec << std::setfill(' ');
            } else {
                output << static_cast<char>(character);
            }
        }
    }
    output << '"';
    return output.str();
}

[[nodiscard]] std::string escape_text(std::string_view value) {
    std::ostringstream output;
    for (const unsigned char character : value) {
        switch (character) {
        case '"':
            output << "\\\"";
            break;
        case '\\':
            output << "\\\\";
            break;
        case '\n':
            output << "\\n";
            break;
        case '\r':
            output << "\\r";
            break;
        case '\t':
            output << "\\t";
            break;
        default:
            if (character < 0x20U) {
                output << "\\x" << std::hex << std::setw(2) << std::setfill('0')
                       << static_cast<unsigned int>(character) << std::dec << std::setfill(' ');
            } else {
                output << static_cast<char>(character);
            }
        }
    }
    return output.str();
}

[[nodiscard]] std::string escape_dot(std::string_view value) {
    std::ostringstream output;
    for (const unsigned char character : value) {
        switch (character) {
        case '"':
            output << "\\\"";
            break;
        case '\\':
            output << "\\\\";
            break;
        case '\n':
            output << "\\n";
            break;
        case '\r':
            output << "\\r";
            break;
        case '\t':
            output << "\\t";
            break;
        default:
            if (character < 0x20U) {
                output << "\\x" << std::hex << std::setw(2) << std::setfill('0')
                       << static_cast<unsigned int>(character) << std::dec << std::setfill(' ');
            } else {
                output << static_cast<char>(character);
            }
        }
    }
    return output.str();
}

void append_string_array(std::ostringstream &output, const std::vector<std::string> &values) {
    output << '[';
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        output << escape_json(values[index]);
    }
    output << ']';
}

[[nodiscard]] std::string encoded_string_array(const std::vector<std::string> &values) {
    std::ostringstream output;
    append_string_array(output, values);
    return output.str();
}

void append_attributes(std::ostringstream &output, const NormalizedModel &model, IndexRange range,
                       bool include_evidence) {
    std::vector<const ModelAttribute *> attributes;
    const auto begin = std::min<std::size_t>(range.begin, model.attributes.size());
    const auto end = std::min<std::size_t>(begin + range.count, model.attributes.size());
    for (auto index = begin; index < end; ++index) {
        const auto &attribute = model.attributes[index];
        if (include_evidence || model.string(attribute.key) != "source-ref") {
            attributes.push_back(&attribute);
        }
    }
    std::sort(attributes.begin(), attributes.end(), [&](const auto *left, const auto *right) {
        const auto left_key = model.string(left->key);
        const auto right_key = model.string(right->key);
        if (left_key != right_key) {
            return left_key < right_key;
        }
        return model.string(left->value) < model.string(right->value);
    });
    output << '[';
    for (std::size_t index = 0; index < attributes.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        const auto &attribute = *attributes[index];
        output << "{\"key\":" << escape_json(model.string(attribute.key))
               << ",\"kind\":" << escape_json(to_string(attribute.kind))
               << ",\"value\":" << escape_json(model.string(attribute.value))
               << ",\"extension\":" << (attribute.extension ? "true" : "false") << '}';
    }
    output << ']';
}

void append_object_reference(std::ostringstream &output, const NormalizedModel &model,
                             ObjectRef object) {
    output << escape_json(model.external_id(object));
}

void append_failure_domains(std::ostringstream &output, const NormalizedModel &model,
                            IndexRange range) {
    std::vector<std::string_view> values;
    const auto begin = std::min<std::size_t>(range.begin, model.object_failure_domains.size());
    const auto end =
        std::min<std::size_t>(begin + range.count, model.object_failure_domains.size());
    for (auto index = begin; index < end; ++index) {
        const auto domain = model.object_failure_domains[index];
        if (domain.is_valid() && domain.value() < model.failure_domains.size()) {
            values.push_back(model.string(model.failure_domains[domain.value()].external_id));
        }
    }
    std::sort(values.begin(), values.end());
    output << '[';
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index != 0) {
            output << ',';
        }
        output << escape_json(values[index]);
    }
    output << ']';
}

[[nodiscard]] std::size_t count_visible(const std::vector<bool> &values) {
    return static_cast<std::size_t>(std::count(values.begin(), values.end(), true));
}

[[nodiscard]] Plane object_plane(const NormalizedModel &model, ObjectRef object) noexcept {
    const auto entity = object.kind == ObjectKind::entity ? EntityId{object.index}
                                                          : model.endpoints[object.index].owner;
    return model.entities[entity.value()].plane;
}

[[nodiscard]] Layer object_layer(const NormalizedModel &model, ObjectRef object) noexcept {
    const auto entity = object.kind == ObjectKind::entity ? EntityId{object.index}
                                                          : model.endpoints[object.index].owner;
    return model.entities[entity.value()].layer;
}

[[nodiscard]] ScopeId object_scope(const NormalizedModel &model, ObjectRef object) noexcept {
    return object.kind == ObjectKind::entity ? model.entities[object.index].scope
                                             : model.endpoints[object.index].scope;
}

[[nodiscard]] bool has_object(const std::vector<ObjectRef> &values, ObjectRef target) {
    return std::find(values.begin(), values.end(), target) != values.end();
}

[[nodiscard]] bool has_relation(const std::vector<RelationId> &values, RelationId target) {
    return std::find(values.begin(), values.end(), target) != values.end();
}

[[nodiscard]] DotSelection select_dot_graph(const NormalizedModel &model,
                                            const Visibility &visibility,
                                            const DotOutputOptions &options) {
    DotSelection selection;
    selection.entities.resize(model.entities.size());
    selection.endpoints.resize(model.endpoints.size());
    selection.relations.resize(model.relations.size());
    const bool focused = !options.focus_objects.empty() || !options.highlighted_relations.empty();

    auto matches_filter = [&](ObjectRef object) {
        if (options.plane.has_value() && object_plane(model, object) != *options.plane) {
            return false;
        }
        if (options.layer.has_value() && object_layer(model, object) != *options.layer) {
            return false;
        }
        return !options.failure_domain.has_value() ||
               object_in_domain(model, object, *options.failure_domain);
    };
    auto requested = [&](ObjectRef object) {
        if (!focused) {
            return true;
        }
        if (has_object(options.focus_objects, object) ||
            has_object(options.highlighted_objects, object)) {
            return true;
        }
        for (const auto relation_id : options.highlighted_relations) {
            if (relation_id.is_valid() && relation_id.value() < model.relations.size()) {
                const auto &relation = model.relations[relation_id.value()];
                if (relation.from == object || relation.to == object) {
                    return true;
                }
            }
        }
        return false;
    };

    for (const auto &entity : model.entities) {
        const auto object = ObjectRef::from(entity.id);
        selection.entities[entity.id.value()] =
            visibility.entities[entity.id.value()] && matches_filter(object) && requested(object);
    }
    for (const auto &endpoint : model.endpoints) {
        const auto object = ObjectRef::from(endpoint.id);
        selection.endpoints[endpoint.id.value()] = visibility.endpoints[endpoint.id.value()] &&
                                                   matches_filter(object) && requested(object);
        if (selection.endpoints[endpoint.id.value()] && endpoint.owner.is_valid() &&
            endpoint.owner.value() < selection.entities.size() &&
            visibility.entities[endpoint.owner.value()]) {
            selection.entities[endpoint.owner.value()] = true;
        }
    }
    for (const auto &relation : model.relations) {
        const bool from_selected = relation.from.kind == ObjectKind::entity
                                       ? selection.entities[relation.from.index]
                                       : selection.endpoints[relation.from.index];
        const bool to_selected = relation.to.kind == ObjectKind::entity
                                     ? selection.entities[relation.to.index]
                                     : selection.endpoints[relation.to.index];
        selection.relations[relation.id.value()] =
            visibility.relations[relation.id.value()] && from_selected && to_selected;
    }
    return selection;
}

[[nodiscard]] std::string plane_color(Plane plane) {
    switch (plane) {
    case Plane::facility:
        return "#b45309";
    case Plane::communication:
        return "#2563eb";
    case Plane::control_signal:
        return "#7c3aed";
    case Plane::service:
        return "#059669";
    }
    return "#4b5563";
}

[[nodiscard]] std::string relation_color(RelationKind kind) {
    switch (kind) {
    case RelationKind::network:
        return "#2563eb";
    case RelationKind::power:
        return "#b45309";
    case RelationKind::serial:
        return "#7c3aed";
    case RelationKind::contact:
        return "#059669";
    case RelationKind::analog_current:
        return "#db2777";
    default:
        return "#4b5563";
    }
}

[[nodiscard]] std::optional<FailureDomainId> first_domain(const NormalizedModel &model,
                                                          ObjectRef object) {
    if (!valid_object(model, object)) {
        return std::nullopt;
    }
    const auto range = object.kind == ObjectKind::entity
                           ? model.entities[object.index].failure_domains
                           : model.endpoints[object.index].failure_domains;
    const auto begin = std::min<std::size_t>(range.begin, model.object_failure_domains.size());
    const auto end =
        std::min<std::size_t>(begin + range.count, model.object_failure_domains.size());
    if (begin == end) {
        return std::nullopt;
    }
    return model.object_failure_domains[begin];
}

[[nodiscard]] std::pair<std::string, std::string>
dot_group(const NormalizedModel &model, ObjectRef object, const DotOutputOptions &options) {
    switch (options.grouping) {
    case DotGrouping::none:
        return {"", ""};
    case DotGrouping::scope: {
        const auto scope = object_scope(model, object);
        if (scope.is_valid() && scope.value() < model.scopes.size()) {
            const auto id = std::string{model.string(model.scopes[scope.value()].external_id)};
            return {"scope:" + id, "scope " + id};
        }
        return {"scope:none", "scope none"};
    }
    case DotGrouping::plane: {
        const auto value = std::string{to_string(object_plane(model, object))};
        return {"plane:" + value, "plane " + value};
    }
    case DotGrouping::layer: {
        const auto value = std::string{to_string(object_layer(model, object))};
        return {"layer:" + value, "layer " + value};
    }
    case DotGrouping::failure_domain: {
        const auto domain = options.failure_domain.has_value() ? options.failure_domain
                                                               : first_domain(model, object);
        if (domain.has_value() && domain->is_valid() &&
            domain->value() < model.failure_domains.size()) {
            const auto id =
                std::string{model.string(model.failure_domains[domain->value()].external_id)};
            return {"failure-domain:" + id, "failure-domain " + id};
        }
        return {"failure-domain:none", "failure-domain none"};
    }
    }
    return {"", ""};
}

[[nodiscard]] std::string dot_node(const NormalizedModel &model, ObjectRef object,
                                   bool highlighted) {
    const auto external_id = model.external_id(object);
    const auto label_id = object.kind == ObjectKind::entity ? model.entities[object.index].label
                                                            : model.endpoints[object.index].label;
    const auto label = label_id.is_valid() ? model.string(label_id) : std::string_view{external_id};
    std::ostringstream output;
    output << "    \"" << escape_dot(external_id) << "\" [label=\""
           << escape_dot(std::string{label} + "\n" + std::string{external_id})
           << "\",shape=" << (object.kind == ObjectKind::entity ? "box" : "ellipse") << ",color=\""
           << plane_color(object_plane(model, object)) << "\",style=\""
           << (highlighted                         ? "filled,bold"
               : object.kind == ObjectKind::entity ? "rounded"
                                                   : "solid")
           << '"';
    if (highlighted) {
        output << ",fillcolor=\"#fef3c7\"";
    }
    output << "];";
    return output.str();
}

} // namespace

std::string render_model_human(const NormalizedModel &model, const ModelOutputContext &context) {
    const auto visibility = build_visibility(model, context);
    const auto model_inputs = sorted_unique_strings(context.model_inputs);
    const auto observation_inputs = sorted_unique_strings(context.observation_inputs);
    std::array<std::size_t, 3> evidence_counts{};
    for (const auto &scope : model.scopes) {
        switch (scope.evidence_quality) {
        case EvidenceQuality::confirmed:
            ++evidence_counts[0];
            break;
        case EvidenceQuality::inferred:
            ++evidence_counts[1];
            break;
        case EvidenceQuality::unknown:
            ++evidence_counts[2];
            break;
        }
    }

    std::ostringstream output;
    output << "result: complete\n"
           << "schema: " << kModelSchema << "\n"
           << "operation: model-summary\n"
           << "focus: registered topology model\n"
           << "confidence: confirmed=" << evidence_counts[0] << " inferred=" << evidence_counts[1]
           << " unknown=" << evidence_counts[2] << "\n"
           << "analysis-at: "
           << (context.analysis_at.has_value() ? escape_text(*context.analysis_at) : "not-set")
           << "\n"
           << "model-inputs:\n";
    if (model_inputs.empty()) {
        output << "  - none\n";
    } else {
        for (const auto &input : model_inputs) {
            output << "  - " << escape_text(input) << "\n";
        }
    }
    output << "observation-inputs:\n";
    if (observation_inputs.empty()) {
        output << "  - none\n";
    } else {
        for (const auto &input : observation_inputs) {
            output << "  - " << escape_text(input) << "\n";
        }
    }
    output << "summary:\n"
           << "  scopes: " << model.scopes.size() << "\n"
           << "  entities: " << count_visible(visibility.entities) << "\n"
           << "  endpoints: " << count_visible(visibility.endpoints) << "\n"
           << "  relations: " << count_visible(visibility.relations) << "\n"
           << "  requirement-groups: " << count_visible(visibility.requirement_groups) << "\n"
           << "  failure-domains: " << count_visible(visibility.failure_domains) << "\n"
           << "  failure-modes: " << count_visible(visibility.failure_modes) << "\n"
           << "  probes: " << count_visible(visibility.probes) << "\n"
           << "scopes:\n";
    for (const auto index : sorted_indices(model, model.scopes)) {
        const auto &scope = model.scopes[index];
        output << "  - " << model.string(scope.external_id) << " kind=" << to_string(scope.kind)
               << " completeness=" << to_string(scope.completeness)
               << " evidence=" << to_string(scope.evidence_quality);
        if (scope.label.is_valid()) {
            output << " label=\"" << escape_text(model.string(scope.label)) << '"';
        }
        output << "\n";
    }
    output << "entities:\n";
    for (const auto index : sorted_indices(model, model.entities)) {
        const auto &entity = model.entities[index];
        if (!visibility.entities[entity.id.value()]) {
            continue;
        }
        output << "  - " << model.string(entity.external_id) << " kind=" << to_string(entity.kind)
               << " plane=" << to_string(entity.plane) << " layer=" << to_string(entity.layer)
               << " scope=" << model.string(model.scopes[entity.scope.value()].external_id);
        if (entity.label.is_valid()) {
            output << " label=\"" << escape_text(model.string(entity.label)) << '"';
        }
        if (context.restricted_policy == RestrictedOutputPolicy::include_explicit &&
            entity.source_ref.is_valid()) {
            output << " source-ref=\"" << escape_text(model.string(entity.source_ref)) << '"';
        }
        output << "\n";
    }
    output << "endpoints:\n";
    for (const auto index : sorted_indices(model, model.endpoints)) {
        const auto &endpoint = model.endpoints[index];
        if (!visibility.endpoints[endpoint.id.value()]) {
            continue;
        }
        output << "  - " << model.string(endpoint.external_id)
               << " owner=" << model.string(model.entities[endpoint.owner.value()].external_id)
               << " kind=" << to_string(endpoint.kind) << "\n";
    }
    output << "relations:\n";
    for (const auto index : sorted_indices(model, model.relations)) {
        const auto &relation = model.relations[index];
        if (!visibility.relations[relation.id.value()]) {
            continue;
        }
        output << "  - " << model.string(relation.external_id) << ' '
               << model.external_id(relation.from)
               << (relation.direction == Direction::bidirectional ? " -- " : " -> ")
               << model.external_id(relation.to) << " kind=" << to_string(relation.kind) << "\n";
    }
    output << "unknowns:\n";
    bool has_unknown = false;
    for (const auto index : sorted_indices(model, model.scopes)) {
        const auto &scope = model.scopes[index];
        if (scope.completeness == Completeness::unknown ||
            scope.evidence_quality == EvidenceQuality::unknown) {
            has_unknown = true;
            output << "  - scope " << model.string(scope.external_id)
                   << " completeness=" << to_string(scope.completeness)
                   << " evidence=" << to_string(scope.evidence_quality) << "\n";
        }
    }
    if (!has_unknown) {
        output << "  - none\n";
    }
    output << "omissions:\n"
           << "  restricted-objects: " << visibility.restricted_omissions << "\n"
           << "  evidence-references: " << visibility.evidence_omissions << "\n";
    return output.str();
}

std::string render_model_json(const NormalizedModel &model, const ModelOutputContext &context) {
    const auto visibility = build_visibility(model, context);
    const auto model_inputs = sorted_unique_strings(context.model_inputs);
    const auto observation_inputs = sorted_unique_strings(context.observation_inputs);
    const bool include_evidence =
        context.restricted_policy == RestrictedOutputPolicy::include_explicit;
    std::ostringstream output;
    output << "{\"schema\":" << escape_json(kModelSchema)
           << ",\"operation\":\"model-summary\",\"analysis_at\":";
    if (context.analysis_at.has_value()) {
        output << escape_json(*context.analysis_at);
    } else {
        output << "null";
    }
    output << ",\"model_inputs\":";
    append_string_array(output, model_inputs);
    output << ",\"observation_inputs\":";
    append_string_array(output, observation_inputs);
    output << ",\"status\":\"complete\",\"findings\":[],\"warnings\":";
    if (visibility.restricted_omissions == 0 && visibility.evidence_omissions == 0) {
        output << "[]";
    } else {
        output << "[\"content omitted by output policy\"]";
    }
    output << ",\"resource_budget\":null,\"omissions\":{\"restricted_objects\":"
           << visibility.restricted_omissions
           << ",\"evidence_references\":" << visibility.evidence_omissions
           << "},\"model\":{\"schema_major_version\":" << model.schema_major_version
           << ",\"failure_mode_catalog_version\":" << model.failure_mode_catalog_version
           << ",\"counts\":{\"scopes\":" << model.scopes.size()
           << ",\"entities\":" << count_visible(visibility.entities)
           << ",\"endpoints\":" << count_visible(visibility.endpoints)
           << ",\"relations\":" << count_visible(visibility.relations)
           << ",\"requirement_groups\":" << count_visible(visibility.requirement_groups)
           << ",\"failure_domains\":" << count_visible(visibility.failure_domains)
           << ",\"failure_modes\":" << count_visible(visibility.failure_modes)
           << ",\"probes\":" << count_visible(visibility.probes) << "},\"scopes\":[";

    bool first = true;
    for (const auto index : sorted_indices(model, model.scopes)) {
        const auto &scope = model.scopes[index];
        if (!first) {
            output << ',';
        }
        first = false;
        output << "{\"id\":" << escape_json(model.string(scope.external_id)) << ",\"label\":"
               << (scope.label.is_valid() ? escape_json(model.string(scope.label)) : "null")
               << ",\"kind\":" << escape_json(to_string(scope.kind))
               << ",\"completeness\":" << escape_json(to_string(scope.completeness))
               << ",\"evidence_quality\":" << escape_json(to_string(scope.evidence_quality))
               << ",\"attributes\":";
        append_attributes(output, model, scope.attributes, include_evidence);
        output << '}';
    }

    output << "],\"entities\":[";
    first = true;
    for (const auto index : sorted_indices(model, model.entities)) {
        const auto &entity = model.entities[index];
        if (!visibility.entities[entity.id.value()]) {
            continue;
        }
        if (!first) {
            output << ',';
        }
        first = false;
        output << "{\"id\":" << escape_json(model.string(entity.external_id)) << ",\"label\":"
               << (entity.label.is_valid() ? escape_json(model.string(entity.label)) : "null")
               << ",\"kind\":" << escape_json(to_string(entity.kind))
               << ",\"plane\":" << escape_json(to_string(entity.plane))
               << ",\"layer\":" << escape_json(to_string(entity.layer)) << ",\"scope\":"
               << escape_json(model.string(model.scopes[entity.scope.value()].external_id))
               << ",\"network_space\":";
        if (entity.network_space.is_valid()) {
            output << escape_json(
                model.string(model.entities[entity.network_space.value()].external_id));
        } else {
            output << "null";
        }
        output << ",\"failure_domains\":";
        append_failure_domains(output, model, entity.failure_domains);
        output << ",\"sensitivity\":" << escape_json(to_string(entity.sensitivity));
        if (include_evidence && entity.source_ref.is_valid()) {
            output << ",\"source_ref\":" << escape_json(model.string(entity.source_ref));
        }
        output << ",\"attributes\":";
        append_attributes(output, model, entity.attributes, include_evidence);
        output << '}';
    }

    output << "],\"endpoints\":[";
    first = true;
    for (const auto index : sorted_indices(model, model.endpoints)) {
        const auto &endpoint = model.endpoints[index];
        if (!visibility.endpoints[endpoint.id.value()]) {
            continue;
        }
        if (!first) {
            output << ',';
        }
        first = false;
        output << "{\"id\":" << escape_json(model.string(endpoint.external_id)) << ",\"label\":"
               << (endpoint.label.is_valid() ? escape_json(model.string(endpoint.label)) : "null")
               << ",\"owner\":"
               << escape_json(model.string(model.entities[endpoint.owner.value()].external_id))
               << ",\"kind\":" << escape_json(to_string(endpoint.kind)) << ",\"scope\":"
               << escape_json(model.string(model.scopes[endpoint.scope.value()].external_id))
               << ",\"network_space\":";
        if (endpoint.network_space.is_valid()) {
            output << escape_json(
                model.string(model.entities[endpoint.network_space.value()].external_id));
        } else {
            output << "null";
        }
        output << ",\"failure_domains\":";
        append_failure_domains(output, model, endpoint.failure_domains);
        output << ",\"sensitivity\":" << escape_json(to_string(endpoint.sensitivity));
        if (include_evidence && endpoint.source_ref.is_valid()) {
            output << ",\"source_ref\":" << escape_json(model.string(endpoint.source_ref));
        }
        output << ",\"attributes\":";
        append_attributes(output, model, endpoint.attributes, include_evidence);
        output << '}';
    }

    output << "],\"relations\":[";
    first = true;
    for (const auto index : sorted_indices(model, model.relations)) {
        const auto &relation = model.relations[index];
        if (!visibility.relations[relation.id.value()]) {
            continue;
        }
        if (!first) {
            output << ',';
        }
        first = false;
        output << "{\"id\":" << escape_json(model.string(relation.external_id)) << ",\"from\":";
        append_object_reference(output, model, relation.from);
        output << ",\"to\":";
        append_object_reference(output, model, relation.to);
        output << ",\"kind\":" << escape_json(to_string(relation.kind))
               << ",\"direction\":" << escape_json(to_string(relation.direction))
               << ",\"attributes\":";
        append_attributes(output, model, relation.attributes, include_evidence);
        output << '}';
    }

    output << "],\"requirement_groups\":[";
    first = true;
    for (const auto index : sorted_indices(model, model.requirement_groups)) {
        const auto &requirement = model.requirement_groups[index];
        if (!visibility.requirement_groups[requirement.id.value()]) {
            continue;
        }
        if (!first) {
            output << ',';
        }
        first = false;
        output << "{\"id\":" << escape_json(model.string(requirement.external_id))
               << ",\"consumer\":"
               << escape_json(
                      model.string(model.entities[requirement.consumer.value()].external_id))
               << ",\"mode\":" << escape_json(to_string(requirement.mode)) << ",\"providers\":[";
        std::vector<std::string_view> providers;
        const auto begin =
            std::min<std::size_t>(requirement.providers.begin, model.requirement_providers.size());
        const auto end = std::min<std::size_t>(begin + requirement.providers.count,
                                               model.requirement_providers.size());
        for (auto provider_index = begin; provider_index < end; ++provider_index) {
            const auto provider = model.requirement_providers[provider_index];
            providers.push_back(model.string(model.entities[provider.value()].external_id));
        }
        std::sort(providers.begin(), providers.end());
        for (std::size_t provider_index = 0; provider_index < providers.size(); ++provider_index) {
            if (provider_index != 0) {
                output << ',';
            }
            output << escape_json(providers[provider_index]);
        }
        output << "],\"hold_up_ms\":" << requirement.hold_up_ms << ",\"attributes\":";
        append_attributes(output, model, requirement.attributes, include_evidence);
        output << '}';
    }

    output << "],\"failure_domains\":[";
    first = true;
    for (const auto index : sorted_indices(model, model.failure_domains)) {
        const auto &domain = model.failure_domains[index];
        if (!visibility.failure_domains[domain.id.value()]) {
            continue;
        }
        if (!first) {
            output << ',';
        }
        first = false;
        output << "{\"id\":" << escape_json(model.string(domain.external_id))
               << ",\"kind\":" << escape_json(to_string(domain.kind)) << ",\"members\":[";
        std::vector<std::string_view> members;
        const auto begin =
            std::min<std::size_t>(domain.members.begin, model.failure_domain_members.size());
        const auto end = std::min<std::size_t>(begin + domain.members.count,
                                               model.failure_domain_members.size());
        for (auto member_index = begin; member_index < end; ++member_index) {
            const auto member = model.failure_domain_members[member_index];
            if (object_visible(visibility, member)) {
                members.push_back(model.external_id(member));
            }
        }
        std::sort(members.begin(), members.end());
        for (std::size_t member_index = 0; member_index < members.size(); ++member_index) {
            if (member_index != 0) {
                output << ',';
            }
            output << escape_json(members[member_index]);
        }
        output << "]}";
    }

    output << "],\"failure_modes\":[";
    first = true;
    for (const auto index : sorted_indices(model, model.failure_modes)) {
        const auto &failure = model.failure_modes[index];
        if (!visibility.failure_modes[failure.id.value()]) {
            continue;
        }
        if (!first) {
            output << ',';
        }
        first = false;
        output << "{\"id\":" << escape_json(model.string(failure.external_id)) << ",\"target\":";
        append_object_reference(output, model, failure.target);
        output << ",\"kind\":" << escape_json(to_string(failure.kind));
        if (failure.kind == FailureModeKind::extension && failure.extension_kind.is_valid()) {
            output << ",\"extension_kind\":" << escape_json(model.string(failure.extension_kind));
        }
        output << ",\"delay_ms\":" << failure.delay_ms << ",\"attributes\":";
        append_attributes(output, model, failure.attributes, include_evidence);
        output << '}';
    }

    output << "],\"probes\":[";
    first = true;
    for (const auto index : sorted_indices(model, model.probes)) {
        const auto &probe = model.probes[index];
        if (!visibility.probes[probe.id.value()]) {
            continue;
        }
        if (!first) {
            output << ',';
        }
        first = false;
        output << "{\"id\":" << escape_json(model.string(probe.external_id)) << ",\"target\":";
        append_object_reference(output, model, probe.target);
        output << ",\"vantage\":";
        append_object_reference(output, model, probe.vantage);
        output << ",\"kind\":" << escape_json(to_string(probe.kind))
               << ",\"valid_for_ms\":" << probe.valid_for_ms << ",\"attributes\":";
        append_attributes(output, model, probe.attributes, include_evidence);
        output << '}';
    }
    output << "]}}\n";
    return output.str();
}

std::string render_model_dot(const NormalizedModel &model, const ModelOutputContext &context,
                             const DotOutputOptions &options) {
    const auto visibility = build_visibility(model, context);
    const auto selection = select_dot_graph(model, visibility, options);
    const auto model_inputs = sorted_unique_strings(context.model_inputs);
    const auto observation_inputs = sorted_unique_strings(context.observation_inputs);
    std::ostringstream output;
    output << "digraph ltfa_model {\n"
           << "  graph [ltfa_schema=\"" << kDotSchema << "\",operation=\"model\",model_inputs=\""
           << escape_dot(encoded_string_array(model_inputs)) << "\",observation_inputs=\""
           << escape_dot(encoded_string_array(observation_inputs)) << "\",analysis_at=\""
           << escape_dot(context.analysis_at.has_value() ? *context.analysis_at : "null")
           << "\",restricted_omissions=\"" << visibility.restricted_omissions << "\"];\n"
           << "  node [fontname=\"sans-serif\"];\n"
           << "  edge [fontname=\"sans-serif\"];\n";

    std::map<std::string, std::pair<std::string, std::vector<std::string>>> groups;
    auto add_node = [&](ObjectRef object) {
        const auto [group_id, group_label] = dot_group(model, object, options);
        const auto highlighted = has_object(options.highlighted_objects, object);
        groups[group_id].first = group_label;
        groups[group_id].second.push_back(dot_node(model, object, highlighted));
    };
    for (const auto index : sorted_indices(model, model.entities)) {
        const auto &entity = model.entities[index];
        if (selection.entities[entity.id.value()]) {
            add_node(ObjectRef::from(entity.id));
        }
    }
    for (const auto index : sorted_indices(model, model.endpoints)) {
        const auto &endpoint = model.endpoints[index];
        if (selection.endpoints[endpoint.id.value()]) {
            add_node(ObjectRef::from(endpoint.id));
        }
    }
    if (options.grouping == DotGrouping::none) {
        for (const auto &line : groups[""].second) {
            output << line << '\n';
        }
    } else {
        for (const auto &[group_id, group] : groups) {
            output << "  subgraph \"cluster:" << escape_dot(group_id) << "\" {\n"
                   << "    label=\"" << escape_dot(group.first) << "\";\n";
            for (const auto &line : group.second) {
                output << line << '\n';
            }
            output << "  }\n";
        }
    }
    for (const auto index : sorted_indices(model, model.endpoints)) {
        const auto &endpoint = model.endpoints[index];
        if (selection.endpoints[endpoint.id.value()] && endpoint.owner.is_valid() &&
            selection.entities[endpoint.owner.value()]) {
            output
                << "  \""
                << escape_dot(model.string(model.entities[endpoint.owner.value()].external_id))
                << "\" -> \"" << escape_dot(model.string(endpoint.external_id))
                << "\" [label=\"owns\",color=\"#9ca3af\",style=\"dashed\",arrowhead=\"none\"];\n";
        }
    }
    for (const auto index : sorted_indices(model, model.relations)) {
        const auto &relation = model.relations[index];
        if (!selection.relations[relation.id.value()]) {
            continue;
        }
        output << "  \"" << escape_dot(model.external_id(relation.from)) << "\" -> \""
               << escape_dot(model.external_id(relation.to)) << "\" [label=\""
               << escape_dot(to_string(relation.kind)) << "\",color=\""
               << relation_color(relation.kind) << "\",style=\""
               << (static_cast<std::size_t>(relation.kind) <=
                           static_cast<std::size_t>(RelationKind::analog_current)
                       ? "solid"
                       : "dashed")
               << '"';
        if (relation.direction == Direction::bidirectional) {
            output << ",dir=\"both\"";
        }
        if (has_relation(options.highlighted_relations, relation.id)) {
            output << ",penwidth=\"3\"";
        }
        output << "];\n";
    }
    output << "}\n";
    return output.str();
}

} // namespace ltfa
