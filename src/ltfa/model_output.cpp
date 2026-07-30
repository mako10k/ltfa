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
constexpr std::string_view kDiagramDotSchema{"ltfa.diagram.dot/v1"};

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

struct SemanticContainment {
    std::vector<std::optional<EntityId>> parents;
    std::vector<bool> spatial_relations;
    std::vector<bool> containers;
    std::vector<bool> anchors;
    std::vector<std::vector<ObjectRef>> children;
};

struct SemanticPorts {
    std::vector<bool> folded;
    std::vector<std::string> names;
    std::vector<std::vector<EndpointId>> by_owner;
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

[[nodiscard]] std::string escape_html(std::string_view value) {
    std::ostringstream output;
    for (const unsigned char character : value) {
        switch (character) {
        case '&':
            output << "&amp;";
            break;
        case '<':
            output << "&lt;";
            break;
        case '>':
            output << "&gt;";
            break;
        case '"':
            output << "&quot;";
            break;
        case '\'':
            output << "&#39;";
            break;
        case '\n':
        case '\r':
            output << "&#10;";
            break;
        default:
            if (character < 0x20U) {
                output << "&#" << static_cast<unsigned int>(character) << ';';
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

[[nodiscard]] std::size_t object_slot(const NormalizedModel &model, ObjectRef object) noexcept {
    return object.kind == ObjectKind::entity ? object.index : model.entities.size() + object.index;
}

[[nodiscard]] bool semantic_relation_visible(DotView view, RelationKind kind) noexcept {
    if (kind == RelationKind::contained_in) {
        return true;
    }
    switch (view) {
    case DotView::raw:
        return true;
    case DotView::topology:
        return kind == RelationKind::network || kind == RelationKind::power ||
               kind == RelationKind::serial || kind == RelationKind::contact ||
               kind == RelationKind::analog_current || kind == RelationKind::member_of;
    case DotView::power:
        return kind == RelationKind::power || kind == RelationKind::powered_by;
    case DotView::dependency:
        return kind == RelationKind::depends_on;
    case DotView::overlay:
        return kind == RelationKind::carried_by || kind == RelationKind::uses_underlay ||
               kind == RelationKind::terminates_on || kind == RelationKind::routes_via;
    }
    return false;
}

[[nodiscard]] SemanticContainment build_semantic_containment(const NormalizedModel &model,
                                                             const DotSelection &selection,
                                                             DotView view) {
    const auto object_count = model.entities.size() + model.endpoints.size();
    SemanticContainment containment;
    containment.parents.resize(object_count);
    containment.spatial_relations.resize(model.relations.size());
    containment.containers.resize(model.entities.size());
    containment.anchors.resize(model.entities.size());
    containment.children.resize(model.entities.size());

    std::vector<std::size_t> parent_counts(object_count);
    std::vector<std::optional<RelationId>> candidate_relations(object_count);
    for (const auto &relation : model.relations) {
        if (!selection.relations[relation.id.value()] ||
            relation.kind != RelationKind::contained_in) {
            continue;
        }
        const auto slot = object_slot(model, relation.from);
        ++parent_counts[slot];
        if (parent_counts[slot] == 1 && relation.direction == Direction::directed &&
            relation.to.kind == ObjectKind::entity &&
            object_scope(model, relation.from) == object_scope(model, relation.to)) {
            containment.parents[slot] = EntityId{relation.to.index};
            candidate_relations[slot] = relation.id;
        } else {
            containment.parents[slot].reset();
            candidate_relations[slot].reset();
        }
    }

    constexpr int kUnknown = 0;
    constexpr int kVisiting = 1;
    constexpr int kValid = 2;
    constexpr int kInvalid = 3;
    std::vector<int> states(model.entities.size(), kUnknown);
    for (std::size_t start = 0; start < model.entities.size(); ++start) {
        if (!selection.entities[start] || states[start] != kUnknown) {
            continue;
        }
        std::vector<std::size_t> path;
        auto current = start;
        auto outcome = kValid;
        while (true) {
            if (states[current] == kValid || states[current] == kInvalid) {
                outcome = states[current];
                break;
            }
            if (states[current] == kVisiting) {
                outcome = kInvalid;
                break;
            }
            states[current] = kVisiting;
            path.push_back(current);
            if (parent_counts[current] != 1 || !containment.parents[current].has_value()) {
                outcome = kValid;
                break;
            }
            current = containment.parents[current]->value();
        }
        for (const auto entity : path) {
            states[entity] = outcome;
        }
    }

    for (std::size_t slot = 0; slot < object_count; ++slot) {
        if (parent_counts[slot] != 1 || !candidate_relations[slot].has_value() ||
            !containment.parents[slot].has_value()) {
            containment.parents[slot].reset();
            continue;
        }
        const bool valid_chain = slot < model.entities.size()
                                     ? states[slot] == kValid
                                     : states[containment.parents[slot]->value()] == kValid;
        if (!valid_chain) {
            containment.parents[slot].reset();
            continue;
        }
        containment.spatial_relations[candidate_relations[slot]->value()] = true;
        const auto parent = containment.parents[slot]->value();
        containment.containers[parent] = true;
        const auto child = slot < model.entities.size()
                               ? ObjectRef::from(EntityId{static_cast<std::uint32_t>(slot)})
                               : ObjectRef::from(EndpointId{
                                     static_cast<std::uint32_t>(slot - model.entities.size())});
        containment.children[parent].push_back(child);
    }

    auto object_less = [&](ObjectRef left, ObjectRef right) {
        const auto left_id = model.external_id(left);
        const auto right_id = model.external_id(right);
        return left_id != right_id ? left_id < right_id
                                   : static_cast<int>(left.kind) < static_cast<int>(right.kind);
    };
    for (auto &children : containment.children) {
        std::sort(children.begin(), children.end(), object_less);
    }

    for (const auto &endpoint : model.endpoints) {
        if (selection.endpoints[endpoint.id.value()] && endpoint.owner.is_valid() &&
            endpoint.owner.value() < containment.containers.size() &&
            containment.containers[endpoint.owner.value()]) {
            containment.anchors[endpoint.owner.value()] = true;
        }
    }
    for (const auto &relation : model.relations) {
        if (!selection.relations[relation.id.value()] ||
            (relation.kind == RelationKind::contained_in &&
             containment.spatial_relations[relation.id.value()]) ||
            !semantic_relation_visible(view, relation.kind)) {
            continue;
        }
        for (const auto object : {relation.from, relation.to}) {
            if (object.kind == ObjectKind::entity && object.index < containment.containers.size() &&
                containment.containers[object.index]) {
                containment.anchors[object.index] = true;
            }
        }
    }
    return containment;
}

[[nodiscard]] SemanticPorts build_semantic_ports(const NormalizedModel &model,
                                                 const DotSelection &selection,
                                                 const SemanticContainment &containment) {
    SemanticPorts ports;
    ports.folded.resize(model.endpoints.size());
    ports.names.resize(model.endpoints.size());
    ports.by_owner.resize(model.entities.size());
    for (const auto index : sorted_indices(model, model.endpoints)) {
        const auto &endpoint = model.endpoints[index];
        if (!selection.endpoints[endpoint.id.value()] || !endpoint.owner.is_valid() ||
            endpoint.owner.value() >= selection.entities.size() ||
            !selection.entities[endpoint.owner.value()]) {
            continue;
        }
        const auto spatial_parent =
            containment.parents[object_slot(model, ObjectRef::from(endpoint.id))];
        if (spatial_parent.has_value() && *spatial_parent != endpoint.owner) {
            continue;
        }
        auto &owner_ports = ports.by_owner[endpoint.owner.value()];
        ports.folded[endpoint.id.value()] = true;
        ports.names[endpoint.id.value()] = "p" + std::to_string(owner_ports.size());
        owner_ports.push_back(endpoint.id);
    }
    return ports;
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

[[nodiscard]] std::string_view semantic_relation_color(RelationKind kind) noexcept {
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
    case RelationKind::member_of:
        return "#64748b";
    case RelationKind::contained_in:
        return "#6b7280";
    case RelationKind::carried_by:
        return "#0891b2";
    case RelationKind::uses_underlay:
        return "#4f46e5";
    case RelationKind::terminates_on:
        return "#0f766e";
    case RelationKind::routes_via:
        return "#1d4ed8";
    case RelationKind::depends_on:
        return "#dc2626";
    case RelationKind::powered_by:
        return "#d97706";
    }
    return "#4b5563";
}

[[nodiscard]] std::string_view semantic_relation_style(RelationKind kind) noexcept {
    switch (kind) {
    case RelationKind::network:
    case RelationKind::power:
    case RelationKind::serial:
    case RelationKind::contact:
    case RelationKind::analog_current:
        return "solid";
    case RelationKind::contained_in:
        return "dotted";
    case RelationKind::member_of:
    case RelationKind::carried_by:
    case RelationKind::uses_underlay:
    case RelationKind::terminates_on:
    case RelationKind::routes_via:
    case RelationKind::depends_on:
    case RelationKind::powered_by:
        return "dashed";
    }
    return "dashed";
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

[[nodiscard]] std::string semantic_anchor_id(const NormalizedModel &model, EntityId entity) {
    return "@ltfa-anchor:" + std::string{model.string(model.entities[entity.value()].external_id)};
}

[[nodiscard]] std::string_view object_label(const NormalizedModel &model,
                                            ObjectRef object) noexcept {
    const auto label_id = object.kind == ObjectKind::entity ? model.entities[object.index].label
                                                            : model.endpoints[object.index].label;
    return label_id.is_valid() ? model.string(label_id) : model.external_id(object);
}

[[nodiscard]] std::string_view semantic_port_label(const NormalizedModel &model,
                                                   EndpointId endpoint) noexcept {
    const auto &value = model.endpoints[endpoint.value()];
    if (value.label.is_valid()) {
        return model.string(value.label);
    }
    const auto external_id = model.string(value.external_id);
    const auto separator = external_id.rfind(':');
    return separator == std::string_view::npos || separator + 1 >= external_id.size()
               ? external_id
               : external_id.substr(separator + 1);
}

[[nodiscard]] std::string semantic_target(const NormalizedModel &model,
                                          const SemanticContainment &containment,
                                          ObjectRef object) {
    if (object.kind == ObjectKind::entity && containment.containers[object.index]) {
        return semantic_anchor_id(model, EntityId{object.index});
    }
    return std::string{model.external_id(object)};
}

void append_semantic_port_table(std::ostringstream &output, const NormalizedModel &model,
                                const DotOutputOptions &options, const SemanticPorts &ports,
                                EntityId owner, bool include_owner_label) {
    const auto owner_object = ObjectRef::from(owner);
    output << "<<TABLE BORDER=\"" << (include_owner_label ? "1" : "0")
           << "\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\" COLOR=\""
           << plane_color(model.entities[owner.value()].plane) << "\">";
    if (include_owner_label) {
        output << "<TR><TD";
        if (has_object(options.highlighted_objects, owner_object)) {
            output << " BGCOLOR=\"#fef3c7\"";
        }
        output << ">" << escape_html(object_label(model, owner_object)) << "</TD></TR>";
    }
    for (const auto endpoint_id : ports.by_owner[owner.value()]) {
        const auto endpoint_object = ObjectRef::from(endpoint_id);
        output << "<TR><TD PORT=\"" << ports.names[endpoint_id.value()] << "\" TOOLTIP=\""
               << escape_html(model.string(model.endpoints[endpoint_id.value()].external_id))
               << '"';
        if (has_object(options.highlighted_objects, endpoint_object)) {
            output << " BGCOLOR=\"#fef3c7\"";
        }
        output << '>' << escape_html(semantic_port_label(model, endpoint_id)) << "</TD></TR>";
    }
    output << "</TABLE>>";
}

[[nodiscard]] std::string semantic_edge_endpoint(const NormalizedModel &model,
                                                 const SemanticContainment &containment,
                                                 const SemanticPorts &ports, ObjectRef object) {
    ObjectRef rendered_object = object;
    std::string_view port;
    if (object.kind == ObjectKind::endpoint && ports.folded[object.index]) {
        rendered_object = ObjectRef::from(model.endpoints[object.index].owner);
        port = ports.names[object.index];
    }
    std::ostringstream output;
    output << '"' << escape_dot(semantic_target(model, containment, rendered_object)) << '"';
    if (!port.empty()) {
        output << ":\"" << escape_dot(port) << '"';
    }
    return output.str();
}

void append_semantic_node(std::ostringstream &output, const NormalizedModel &model,
                          const DotOutputOptions &options, const SemanticPorts &ports,
                          ObjectRef object, std::size_t indent) {
    const auto highlighted = has_object(options.highlighted_objects, object);
    if (object.kind == ObjectKind::entity && !ports.by_owner[object.index].empty()) {
        output << std::string(indent, ' ') << '"' << escape_dot(model.external_id(object))
               << "\" [label=";
        append_semantic_port_table(output, model, options, ports, EntityId{object.index}, true);
        output << ",tooltip=\"" << escape_dot(model.external_id(object))
               << "\",ltfa_kind=\"entity\",shape=plain];\n";
        return;
    }
    output << std::string(indent, ' ') << '"' << escape_dot(model.external_id(object))
           << "\" [label=\"" << escape_dot(object_label(model, object)) << "\",tooltip=\""
           << escape_dot(model.external_id(object)) << "\",ltfa_kind=\"" << to_string(object.kind)
           << "\",shape=" << (object.kind == ObjectKind::entity ? "box" : "ellipse") << ",color=\""
           << plane_color(object_plane(model, object)) << "\",style=\""
           << (highlighted                         ? "filled,bold"
               : object.kind == ObjectKind::entity ? "rounded"
                                                   : "solid")
           << '"';
    if (highlighted) {
        output << ",fillcolor=\"#fef3c7\"";
    }
    output << "];\n";
}

void append_semantic_tree(std::ostringstream &output, const NormalizedModel &model,
                          const DotOutputOptions &options, const SemanticContainment &containment,
                          const SemanticPorts &ports, const std::vector<ObjectRef> &roots,
                          std::size_t indent) {
    enum class EventKind {
        object,
        close_cluster,
    };
    struct Event {
        EventKind kind{EventKind::object};
        ObjectRef object;
        std::size_t indent{0};
    };

    std::vector<Event> events;
    for (auto iterator = roots.rbegin(); iterator != roots.rend(); ++iterator) {
        events.push_back({EventKind::object, *iterator, indent});
    }
    while (!events.empty()) {
        const auto event = events.back();
        events.pop_back();
        if (event.kind == EventKind::close_cluster) {
            output << std::string(event.indent, ' ') << "}\n";
            continue;
        }
        const auto object = event.object;
        if (object.kind == ObjectKind::endpoint && ports.folded[object.index]) {
            continue;
        }
        if (object.kind != ObjectKind::entity || !containment.containers[object.index]) {
            append_semantic_node(output, model, options, ports, object, event.indent);
            continue;
        }

        const auto entity_id = EntityId{object.index};
        const auto &entity = model.entities[object.index];
        output << std::string(event.indent, ' ')
               << "subgraph \"cluster:container:" << escape_dot(model.string(entity.external_id))
               << "\" {\n"
               << std::string(event.indent + 2, ' ') << "label=\""
               << escape_dot(object_label(model, object)) << "\";\n"
               << std::string(event.indent + 2, ' ') << "tooltip=\""
               << escape_dot(model.string(entity.external_id)) << "\";\n"
               << std::string(event.indent + 2, ' ') << "color=\"" << plane_color(entity.plane)
               << "\";\n"
               << std::string(event.indent + 2, ' ') << "style=\"rounded\";\n";
        if (!ports.by_owner[object.index].empty()) {
            output << std::string(event.indent + 2, ' ') << '"'
                   << escape_dot(semantic_anchor_id(model, entity_id)) << "\" [label=";
            append_semantic_port_table(output, model, options, ports, entity_id, false);
            output << ",tooltip=\"" << escape_dot(model.string(entity.external_id))
                   << "\",ltfa_kind=\"container-anchor\",shape=plain];\n";
        } else if (containment.anchors[object.index]) {
            output
                << std::string(event.indent + 2, ' ') << '"'
                << escape_dot(semantic_anchor_id(model, entity_id)) << "\" [label=\"\",tooltip=\""
                << escape_dot(model.string(entity.external_id))
                << "\",ltfa_kind=\"container-anchor\",shape=point,width=\"0.01\",height=\"0.01\","
                   "style=invis];\n";
        }
        events.push_back({EventKind::close_cluster, {}, event.indent});
        const auto &children = containment.children[object.index];
        for (auto iterator = children.rbegin(); iterator != children.rend(); ++iterator) {
            events.push_back({EventKind::object, *iterator, event.indent + 2});
        }
    }
}

[[nodiscard]] std::string render_semantic_dot(const NormalizedModel &model,
                                              const ModelOutputContext &context,
                                              const DotOutputOptions &options,
                                              const Visibility &visibility,
                                              const DotSelection &selection) {
    const auto containment = build_semantic_containment(model, selection, options.view);
    const auto ports = build_semantic_ports(model, selection, containment);
    const auto model_inputs = sorted_unique_strings(context.model_inputs);
    const auto observation_inputs = sorted_unique_strings(context.observation_inputs);
    std::ostringstream output;
    output << "digraph ltfa_diagram {\n"
           << "  graph [ltfa_schema=\"" << kDiagramDotSchema << "\",operation=\"model\",view=\""
           << to_string(options.view) << "\",model_inputs=\""
           << escape_dot(encoded_string_array(model_inputs)) << "\",observation_inputs=\""
           << escape_dot(encoded_string_array(observation_inputs)) << "\",analysis_at=\""
           << escape_dot(context.analysis_at.has_value() ? *context.analysis_at : "null")
           << "\",restricted_omissions=\"" << visibility.restricted_omissions
           << "\",compound=\"true\",newrank=\"true\",rankdir=\"LR\"];\n"
           << "  node [fontname=\"sans-serif\"];\n"
           << "  edge [fontname=\"sans-serif\"];\n";

    std::map<std::string, std::vector<ObjectRef>> roots_by_scope;
    auto add_root = [&](ObjectRef object) {
        if (!containment.parents[object_slot(model, object)].has_value()) {
            const auto scope = object_scope(model, object);
            const auto scope_id =
                scope.is_valid() && scope.value() < model.scopes.size()
                    ? std::string{model.string(model.scopes[scope.value()].external_id)}
                    : std::string{"none"};
            roots_by_scope[scope_id].push_back(object);
        }
    };
    for (const auto index : sorted_indices(model, model.entities)) {
        const auto &entity = model.entities[index];
        if (selection.entities[entity.id.value()]) {
            add_root(ObjectRef::from(entity.id));
        }
    }
    for (const auto index : sorted_indices(model, model.endpoints)) {
        const auto &endpoint = model.endpoints[index];
        if (selection.endpoints[endpoint.id.value()] && !ports.folded[endpoint.id.value()]) {
            add_root(ObjectRef::from(endpoint.id));
        }
    }
    auto object_less = [&](ObjectRef left, ObjectRef right) {
        const auto left_id = model.external_id(left);
        const auto right_id = model.external_id(right);
        return left_id != right_id ? left_id < right_id
                                   : static_cast<int>(left.kind) < static_cast<int>(right.kind);
    };
    for (auto &[scope_id, roots] : roots_by_scope) {
        std::sort(roots.begin(), roots.end(), object_less);
        const auto scope =
            std::find_if(model.scopes.begin(), model.scopes.end(), [&](const auto &candidate) {
                return model.string(candidate.external_id) == scope_id;
            });
        const auto label = scope != model.scopes.end() && scope->label.is_valid()
                               ? model.string(scope->label)
                               : std::string_view{scope_id};
        output << "  subgraph \"cluster:scope:" << escape_dot(scope_id) << "\" {\n"
               << "    label=\"" << escape_dot(label) << "\";\n"
               << "    tooltip=\"" << escape_dot(scope_id) << "\";\n"
               << "    color=\"#9ca3af\";\n"
               << "    style=\"rounded\";\n";
        append_semantic_tree(output, model, options, containment, ports, roots, 4);
        output << "  }\n";
    }

    for (const auto index : sorted_indices(model, model.endpoints)) {
        const auto &endpoint = model.endpoints[index];
        if (!selection.endpoints[endpoint.id.value()] || ports.folded[endpoint.id.value()] ||
            !endpoint.owner.is_valid() || !selection.entities[endpoint.owner.value()]) {
            continue;
        }
        const auto owner = ObjectRef::from(endpoint.owner);
        output << "  " << semantic_edge_endpoint(model, containment, ports, owner) << " -> "
               << semantic_edge_endpoint(model, containment, ports, ObjectRef::from(endpoint.id))
               << " [label=\"owns\",tooltip=\"owns\",ltfa_kind=\"owns\",color=\"#9ca3af\","
                  "style=\"dashed\",arrowhead=\"none\"];\n";
    }
    for (const auto index : sorted_indices(model, model.relations)) {
        const auto &relation = model.relations[index];
        if (!selection.relations[relation.id.value()] ||
            (relation.kind == RelationKind::contained_in &&
             containment.spatial_relations[relation.id.value()]) ||
            !semantic_relation_visible(options.view, relation.kind)) {
            continue;
        }
        const bool structural_fallback = relation.kind == RelationKind::contained_in;
        output << "  " << semantic_edge_endpoint(model, containment, ports, relation.from) << " -> "
               << semantic_edge_endpoint(model, containment, ports, relation.to) << " [";
        if (structural_fallback) {
            output << "label=\"contained_in\",";
        }
        output << "tooltip=\"" << escape_dot(model.string(relation.external_id)) << " ("
               << escape_dot(to_string(relation.kind)) << ")\",ltfa_relation=\""
               << escape_dot(model.string(relation.external_id)) << "\",ltfa_kind=\""
               << escape_dot(to_string(relation.kind)) << "\",color=\""
               << semantic_relation_color(relation.kind) << "\",style=\""
               << semantic_relation_style(relation.kind) << '"';
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

} // namespace

std::string_view to_string(DotView value) noexcept {
    switch (value) {
    case DotView::raw:
        return "raw";
    case DotView::topology:
        return "topology";
    case DotView::power:
        return "power";
    case DotView::dependency:
        return "dependency";
    case DotView::overlay:
        return "overlay";
    }
    return "raw";
}

std::optional<DotView> parse_dot_view(std::string_view value) noexcept {
    if (value == "raw") {
        return DotView::raw;
    }
    if (value == "topology") {
        return DotView::topology;
    }
    if (value == "power") {
        return DotView::power;
    }
    if (value == "dependency") {
        return DotView::dependency;
    }
    if (value == "overlay") {
        return DotView::overlay;
    }
    return std::nullopt;
}

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
    if (options.view != DotView::raw) {
        return render_semantic_dot(model, context, options, visibility, selection);
    }
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
