#include "ltfa/model_compiler.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace ltfa {
namespace {

using dsl::Attribute;
using dsl::ParsedValue;

struct DomainReference {
    std::string id;
    SourceSpan source;
};

struct PendingRelation {
    const std::vector<Attribute> *attributes{nullptr};
    std::string external_id;
    ObjectRef from;
    ObjectRef to;
    RelationKind kind{RelationKind::depends_on};
    Direction direction{Direction::directed};
    SourceSpan source;
};

struct PendingRequirement {
    std::string external_id;
    EntityId consumer;
    RequirementMode mode{RequirementMode::all};
    std::uint64_t hold_up_ms{0};
    SourceSpan source;
    std::map<std::string, std::pair<EntityId, SourceSpan>> providers;
    std::vector<const Attribute *> extensions;
};

struct PendingDependency {
    ObjectRef provider;
    ObjectRef consumer;
    RelationId relation;
    RequirementGroupId requirement_group;
    SourceSpan source;
};

[[nodiscard]] bool is_ascii_lower(char value) noexcept { return value >= 'a' && value <= 'z'; }

[[nodiscard]] bool is_ascii_digit(char value) noexcept { return value >= '0' && value <= '9'; }

[[nodiscard]] bool valid_external_id(std::string_view value) noexcept {
    if (value.empty() || (!is_ascii_lower(value.front()) && !is_ascii_digit(value.front()))) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](char character) {
        return is_ascii_lower(character) || is_ascii_digit(character) || character == '.' ||
               character == '_' || character == ':' || character == '-';
    });
}

[[nodiscard]] std::string encoded_id_component(std::string_view value) {
    return std::to_string(value.size()) + "." + std::string{value};
}

[[nodiscard]] std::size_t relation_kind_index(RelationKind kind) noexcept {
    return static_cast<std::size_t>(kind);
}

[[nodiscard]] AttributeValueKind attribute_kind(dsl::ValueKind kind) noexcept {
    switch (kind) {
    case dsl::ValueKind::identifier:
        return AttributeValueKind::identifier;
    case dsl::ValueKind::string:
        return AttributeValueKind::string;
    case dsl::ValueKind::integer:
        return AttributeValueKind::integer;
    case dsl::ValueKind::real:
        return AttributeValueKind::real;
    case dsl::ValueKind::boolean:
        return AttributeValueKind::boolean;
    case dsl::ValueKind::duration:
        return AttributeValueKind::duration;
    }
    return AttributeValueKind::identifier;
}

[[nodiscard]] const Attribute *find_attribute(const std::vector<Attribute> &attributes,
                                              std::string_view key) noexcept {
    const auto found =
        std::find_if(attributes.begin(), attributes.end(),
                     [key](const Attribute &attribute) { return attribute.key == key; });
    return found == attributes.end() ? nullptr : &*found;
}

[[nodiscard]] bool is_consumed(std::string_view key,
                               std::initializer_list<std::string_view> consumed) noexcept {
    return std::find(consumed.begin(), consumed.end(), key) != consumed.end();
}

class Compiler {
  public:
    Compiler(const dsl::LoadedDocument &document, const ModelLimits &limits)
        : document_(document), limits_(limits) {}

    [[nodiscard]] CompileResult run() {
        if (document_.total_source_bytes > limits_.max_total_source_bytes) {
            fail(DiagnosticCode::limit_total_source_bytes, first_source(),
                 "loaded source bytes exceed limit of " +
                     std::to_string(limits_.max_total_source_bytes));
            return {std::nullopt, std::move(error_)};
        }
        if (!collect_declarations() || !compile_scopes() || !compile_entities() ||
            !compile_endpoints() || !validate_network_spaces() || !compile_failure_domains() ||
            !compile_relations() || !compile_requirements() || !compile_failures() ||
            !compile_probes() || !build_relation_adjacency() ||
            !build_availability_dependencies()) {
            return {std::nullopt, std::move(error_)};
        }
        return {std::move(model_), std::nullopt};
    }

  private:
    bool fail(DiagnosticCode code, SourceSpan source, std::string message,
              std::optional<SourceSpan> secondary = std::nullopt) {
        if (!error_.has_value()) {
            error_ = Diagnostic{code,         DiagnosticSeverity::error, source, secondary,
                                std::nullopt, std::move(message)};
        }
        return false;
    }

    [[nodiscard]] StringId intern(std::string_view value) {
        if (value.empty()) {
            return {};
        }
        const auto found = interned_strings_.find(value);
        if (found != interned_strings_.end()) {
            return found->second;
        }
        const auto id = StringId{static_cast<std::uint32_t>(model_.strings.size())};
        model_.strings.emplace_back(value);
        interned_strings_.emplace(model_.strings.back(), id);
        return id;
    }

    [[nodiscard]] bool validate_id(const ParsedValue &value, std::string_view description) {
        if (value.kind != dsl::ValueKind::identifier || !valid_external_id(value.text)) {
            return fail(DiagnosticCode::id_invalid, value.source,
                        std::string{description} + " must match [a-z0-9][a-z0-9._:-]*");
        }
        return true;
    }

    template <typename Statement>
    [[nodiscard]] bool add_declaration(std::map<std::string, const Statement *> &declarations,
                                       const Statement &statement, std::string_view description) {
        if (!validate_id(statement.id, description)) {
            return false;
        }
        const auto [existing, inserted] = declarations.emplace(statement.id.text, &statement);
        if (!inserted) {
            return fail(DiagnosticCode::id_duplicate, statement.id.source,
                        std::string{description} + " '" + statement.id.text + "' is duplicated",
                        existing->second->id.source);
        }
        return true;
    }

    [[nodiscard]] bool reserve_stable_id(std::string_view external_id, SourceSpan source,
                                         std::string_view description) {
        const auto [existing, inserted] = stable_ids_.emplace(std::string{external_id}, source);
        if (!inserted) {
            return fail(DiagnosticCode::id_duplicate, source,
                        std::string{description} + " '" + std::string{external_id} +
                            "' duplicates another stable ID",
                        existing->second);
        }
        return true;
    }

    [[nodiscard]] bool collect_declarations() {
        std::uint64_t object_count = 0;
        std::uint64_t relation_count = 0;

        for (const auto &statement : document_.statements) {
            if (const auto *scope = std::get_if<dsl::ScopeStatement>(&statement)) {
                if (!add_declaration(scope_statements_, *scope, "scope ID")) {
                    return false;
                }
                if (!reserve_stable_id(scope->id.text, scope->id.source, "scope ID")) {
                    return false;
                }
            } else if (const auto *entity = std::get_if<dsl::EntityStatement>(&statement)) {
                if (!add_declaration(entity_statements_, *entity, "entity ID")) {
                    return false;
                }
                if (!reserve_stable_id(entity->id.text, entity->id.source, "entity ID")) {
                    return false;
                }
                ++object_count;
            } else if (const auto *endpoint = std::get_if<dsl::EndpointStatement>(&statement)) {
                if (!add_declaration(endpoint_statements_, *endpoint, "endpoint ID")) {
                    return false;
                }
                if (!reserve_stable_id(endpoint->id.text, endpoint->id.source, "endpoint ID")) {
                    return false;
                }
                ++object_count;
            } else if (const auto *failure = std::get_if<dsl::FailureStatement>(&statement)) {
                if (!add_declaration(failure_statements_, *failure, "failure ID")) {
                    return false;
                }
                if (!reserve_stable_id(failure->id.text, failure->id.source, "failure ID")) {
                    return false;
                }
            } else if (const auto *probe = std::get_if<dsl::ProbeStatement>(&statement)) {
                if (!add_declaration(probe_statements_, *probe, "probe ID")) {
                    return false;
                }
                if (!reserve_stable_id(probe->id.text, probe->id.source, "probe ID")) {
                    return false;
                }
            } else if (std::holds_alternative<dsl::ConnectStatement>(statement) ||
                       std::holds_alternative<dsl::RelateStatement>(statement)) {
                ++relation_count;
            } else if (std::holds_alternative<dsl::IncludeStatement>(statement)) {
                return fail(DiagnosticCode::include_invalid,
                            std::get<dsl::IncludeStatement>(statement).source,
                            "loaded document must not contain unresolved include statements");
            }
        }

        if (object_count > limits_.max_entities) {
            return fail(DiagnosticCode::limit_exceeded, first_source(),
                        "entity and endpoint count exceeds limit of " +
                            std::to_string(limits_.max_entities));
        }
        if (relation_count > limits_.max_relations) {
            return fail(DiagnosticCode::limit_exceeded, first_source(),
                        "relation count exceeds limit of " + std::to_string(limits_.max_relations));
        }
        if (object_count >= EntityId::invalid_value()) {
            return fail(DiagnosticCode::limit_exceeded, first_source(),
                        "object count exceeds the normalized index range");
        }

        for (const auto &[external_id, statement] : entity_statements_) {
            const auto id = EntityId{static_cast<std::uint32_t>(entity_ids_.size())};
            entity_ids_.emplace(external_id, id);
            object_ids_.emplace(external_id, ObjectRef::from(id));
            (void)statement;
        }
        for (const auto &[external_id, statement] : endpoint_statements_) {
            const auto id = EndpointId{static_cast<std::uint32_t>(endpoint_ids_.size())};
            endpoint_ids_.emplace(external_id, id);
            object_ids_.emplace(external_id, ObjectRef::from(id));
            (void)statement;
        }
        return true;
    }

    [[nodiscard]] SourceSpan first_source() const noexcept {
        if (document_.statements.empty()) {
            return {};
        }
        return std::visit([](const auto &statement) { return statement.source; },
                          document_.statements.front());
    }

    [[nodiscard]] bool expect_identifier(const Attribute &attribute) {
        if (attribute.value.kind != dsl::ValueKind::identifier) {
            return fail(DiagnosticCode::dsl_invalid_value, attribute.value.source,
                        "attribute '" + attribute.key + "' must be an identifier");
        }
        return true;
    }

    [[nodiscard]] const Attribute *required_attribute(const std::vector<Attribute> &attributes,
                                                      std::string_view key,
                                                      SourceSpan statement_source) {
        const auto *attribute = find_attribute(attributes, key);
        if (attribute == nullptr) {
            fail(DiagnosticCode::dsl_missing_required_field, statement_source,
                 "missing required attribute '" + std::string{key} + "'");
        }
        return attribute;
    }

    template <typename Value, std::size_t Size>
    [[nodiscard]] std::optional<Value>
    parse_enum(const Attribute &attribute,
               const std::array<std::pair<std::string_view, Value>, Size> &values) {
        if (!expect_identifier(attribute)) {
            return std::nullopt;
        }
        const auto found = std::find_if(values.begin(), values.end(), [&](const auto &candidate) {
            return candidate.first == attribute.value.text;
        });
        if (found == values.end()) {
            fail(DiagnosticCode::dsl_invalid_value, attribute.value.source,
                 "attribute '" + attribute.key + "' has unsupported value '" +
                     attribute.value.text + "'");
            return std::nullopt;
        }
        return found->second;
    }

    [[nodiscard]] bool check_attribute_count(const std::vector<Attribute> &attributes,
                                             SourceSpan source) {
        if (attributes.size() > limits_.max_attributes_per_object) {
            return fail(DiagnosticCode::limit_exceeded, source,
                        "statement exceeds attribute limit of " +
                            std::to_string(limits_.max_attributes_per_object));
        }
        for (const auto &attribute : attributes) {
            if (attribute.value.text.size() > limits_.max_attribute_string_bytes) {
                return fail(DiagnosticCode::limit_exceeded, attribute.value.source,
                            "attribute value exceeds byte limit of " +
                                std::to_string(limits_.max_attribute_string_bytes));
            }
        }
        return true;
    }

    [[nodiscard]] IndexRange append_attributes(const std::vector<Attribute> &attributes,
                                               std::initializer_list<std::string_view> consumed) {
        std::vector<const Attribute *> retained;
        for (const auto &attribute : attributes) {
            if (!is_consumed(attribute.key, consumed)) {
                retained.push_back(&attribute);
            }
        }
        std::stable_sort(retained.begin(), retained.end(), [](const auto *left, const auto *right) {
            if (left->key != right->key) {
                return left->key < right->key;
            }
            return left->value.text < right->value.text;
        });

        const auto begin = static_cast<std::uint32_t>(model_.attributes.size());
        for (const auto *attribute : retained) {
            model_.attributes.push_back({intern(attribute->key), intern(attribute->value.text),
                                         attribute_kind(attribute->value.kind),
                                         attribute->extension, attribute->source});
        }
        return {begin, static_cast<std::uint32_t>(retained.size())};
    }

    [[nodiscard]] bool compile_scopes() {
        static constexpr std::array scope_kinds{
            std::pair{std::string_view{"site"}, ScopeKind::site},
            std::pair{std::string_view{"cloud"}, ScopeKind::cloud},
            std::pair{std::string_view{"shared"}, ScopeKind::shared},
            std::pair{std::string_view{"other"}, ScopeKind::other},
        };
        static constexpr std::array completeness_values{
            std::pair{std::string_view{"verified"}, Completeness::verified},
            std::pair{std::string_view{"partial"}, Completeness::partial},
            std::pair{std::string_view{"unknown"}, Completeness::unknown},
        };
        static constexpr std::array evidence_values{
            std::pair{std::string_view{"confirmed"}, EvidenceQuality::confirmed},
            std::pair{std::string_view{"inferred"}, EvidenceQuality::inferred},
            std::pair{std::string_view{"unknown"}, EvidenceQuality::unknown},
        };

        for (const auto &[external_id, statement] : scope_statements_) {
            if (!check_attribute_count(statement->attributes, statement->source)) {
                return false;
            }
            Scope value;
            value.id = ScopeId{static_cast<std::uint32_t>(model_.scopes.size())};
            value.external_id = intern(external_id);
            value.source = statement->source;
            if (const auto *attribute = find_attribute(statement->attributes, "kind")) {
                const auto parsed = parse_enum(*attribute, scope_kinds);
                if (!parsed.has_value()) {
                    return false;
                }
                value.kind = *parsed;
            }
            if (const auto *attribute = find_attribute(statement->attributes, "label")) {
                value.label = intern(attribute->value.text);
            }
            if (const auto *attribute = find_attribute(statement->attributes, "completeness")) {
                const auto parsed = parse_enum(*attribute, completeness_values);
                if (!parsed.has_value()) {
                    return false;
                }
                value.completeness = *parsed;
            }
            if (const auto *attribute = find_attribute(statement->attributes, "evidence-quality")) {
                const auto parsed = parse_enum(*attribute, evidence_values);
                if (!parsed.has_value()) {
                    return false;
                }
                value.evidence_quality = *parsed;
            }
            value.attributes = append_attributes(
                statement->attributes, {"kind", "label", "completeness", "evidence-quality"});
            scope_ids_.emplace(external_id, value.id);
            model_.scopes.push_back(value);
        }
        return true;
    }

    [[nodiscard]] std::optional<ScopeId> resolve_scope(const std::vector<Attribute> &attributes,
                                                       SourceSpan source) {
        const auto *attribute = find_attribute(attributes, "scope");
        if (attribute == nullptr) {
            if (model_.scopes.size() == 1) {
                return ScopeId{0};
            }
            if (model_.scopes.empty()) {
                fail(DiagnosticCode::reference_unresolved, source,
                     "object requires a declared scope");
            } else {
                fail(DiagnosticCode::dsl_ambiguous_scope, source,
                     "object must name scope= when multiple scopes are declared");
            }
            return std::nullopt;
        }
        if (!expect_identifier(*attribute)) {
            return std::nullopt;
        }
        const auto found = scope_ids_.find(attribute->value.text);
        if (found == scope_ids_.end()) {
            fail(DiagnosticCode::reference_unresolved, attribute->value.source,
                 "scope '" + attribute->value.text + "' is not declared");
            return std::nullopt;
        }
        return found->second;
    }

    [[nodiscard]] bool validate_plane_layer(const Entity &entity, SourceSpan source) {
        const bool valid =
            ((entity.plane == Plane::facility || entity.plane == Plane::control_signal) &&
             (entity.layer == Layer::none || entity.layer == Layer::physical)) ||
            entity.plane == Plane::communication ||
            (entity.plane == Plane::service &&
             (entity.layer == Layer::none || entity.layer == Layer::application));
        if (!valid) {
            return fail(DiagnosticCode::layer_incompatible, source,
                        "plane '" + std::string{to_string(entity.plane)} +
                            "' is incompatible with "
                            "layer '" +
                            std::string{to_string(entity.layer)} + "'");
        }
        return true;
    }

    [[nodiscard]] bool compile_entities() {
        static constexpr std::array entity_kinds{
            std::pair{std::string_view{"device"}, EntityKind::device},
            std::pair{std::string_view{"facility"}, EntityKind::facility},
            std::pair{std::string_view{"bus"}, EntityKind::bus},
            std::pair{std::string_view{"network-space"}, EntityKind::network_space},
            std::pair{std::string_view{"tunnel"}, EntityKind::tunnel},
            std::pair{std::string_view{"service"}, EntityKind::service},
            std::pair{std::string_view{"process"}, EntityKind::process},
            std::pair{std::string_view{"other"}, EntityKind::other},
        };
        static constexpr std::array planes{
            std::pair{std::string_view{"facility"}, Plane::facility},
            std::pair{std::string_view{"communication"}, Plane::communication},
            std::pair{std::string_view{"control-signal"}, Plane::control_signal},
            std::pair{std::string_view{"service"}, Plane::service},
        };
        static constexpr std::array layers{
            std::pair{std::string_view{"none"}, Layer::none},
            std::pair{std::string_view{"physical"}, Layer::physical},
            std::pair{std::string_view{"l2"}, Layer::l2},
            std::pair{std::string_view{"l3"}, Layer::l3},
            std::pair{std::string_view{"overlay"}, Layer::overlay},
            std::pair{std::string_view{"virtual-network"}, Layer::virtual_network},
            std::pair{std::string_view{"transport"}, Layer::transport},
            std::pair{std::string_view{"application"}, Layer::application},
        };
        static constexpr std::array sensitivities{
            std::pair{std::string_view{"internal"}, Sensitivity::internal},
            std::pair{std::string_view{"restricted"}, Sensitivity::restricted},
        };

        entity_domains_.resize(entity_statements_.size());
        for (const auto &[external_id, statement] : entity_statements_) {
            if (!check_attribute_count(statement->attributes, statement->source)) {
                return false;
            }
            Entity value;
            value.id = entity_ids_.at(external_id);
            value.external_id = intern(external_id);
            value.source = statement->source;
            if (const auto *attribute = find_attribute(statement->attributes, "kind")) {
                const auto parsed = parse_enum(*attribute, entity_kinds);
                if (!parsed.has_value()) {
                    return false;
                }
                value.kind = *parsed;
            }
            if (const auto *attribute = find_attribute(statement->attributes, "plane")) {
                const auto parsed = parse_enum(*attribute, planes);
                if (!parsed.has_value()) {
                    return false;
                }
                value.plane = *parsed;
            }
            if (const auto *attribute = find_attribute(statement->attributes, "layer")) {
                const auto parsed = parse_enum(*attribute, layers);
                if (!parsed.has_value()) {
                    return false;
                }
                value.layer = *parsed;
            }
            const auto scope = resolve_scope(statement->attributes, statement->source);
            if (!scope.has_value()) {
                return false;
            }
            value.scope = *scope;
            if (const auto *attribute = find_attribute(statement->attributes, "label")) {
                value.label = intern(attribute->value.text);
            }
            if (const auto *attribute = find_attribute(statement->attributes, "network-space")) {
                if (!expect_identifier(*attribute)) {
                    return false;
                }
                const auto found = entity_ids_.find(attribute->value.text);
                if (found == entity_ids_.end()) {
                    return fail(DiagnosticCode::reference_unresolved, attribute->value.source,
                                "network-space '" + attribute->value.text + "' is not declared");
                }
                value.network_space = found->second;
            }
            if (const auto *attribute = find_attribute(statement->attributes, "sensitivity")) {
                const auto parsed = parse_enum(*attribute, sensitivities);
                if (!parsed.has_value()) {
                    return false;
                }
                value.sensitivity = *parsed;
            }
            if (const auto *attribute = find_attribute(statement->attributes, "source-ref")) {
                value.source_ref = intern(attribute->value.text);
            }
            if (find_attribute(statement->attributes, "address") != nullptr &&
                !value.network_space.is_valid()) {
                return fail(DiagnosticCode::dsl_missing_required_field, statement->source,
                            "entity with address= must name network-space=");
            }
            if (!validate_plane_layer(value, statement->source)) {
                return false;
            }
            for (const auto &attribute : statement->attributes) {
                if (attribute.key == "failure-domain") {
                    if (!expect_identifier(attribute)) {
                        return false;
                    }
                    entity_domains_[value.id.value()].push_back(
                        {attribute.value.text, attribute.value.source});
                }
            }
            value.attributes = append_attributes(
                statement->attributes, {"kind", "plane", "layer", "scope", "label", "network-space",
                                        "failure-domain", "sensitivity", "source-ref"});
            model_.entities.push_back(value);
        }
        return true;
    }

    [[nodiscard]] std::optional<EntityId> resolve_entity(const ParsedValue &value,
                                                         std::string_view role) {
        if (!validate_id(value, role)) {
            return std::nullopt;
        }
        const auto found = entity_ids_.find(value.text);
        if (found == entity_ids_.end()) {
            fail(DiagnosticCode::reference_unresolved, value.source,
                 std::string{role} + " '" + value.text + "' is not a declared entity");
            return std::nullopt;
        }
        return found->second;
    }

    [[nodiscard]] std::optional<ObjectRef> resolve_object(const ParsedValue &value,
                                                          std::string_view role) {
        if (!validate_id(value, role)) {
            return std::nullopt;
        }
        const auto found = object_ids_.find(value.text);
        if (found == object_ids_.end()) {
            fail(DiagnosticCode::reference_unresolved, value.source,
                 std::string{role} + " '" + value.text + "' is not declared");
            return std::nullopt;
        }
        return found->second;
    }

    [[nodiscard]] bool compile_endpoints() {
        static constexpr std::array endpoint_kinds{
            std::pair{std::string_view{"network"}, EndpointKind::network},
            std::pair{std::string_view{"power-input"}, EndpointKind::power_input},
            std::pair{std::string_view{"power-output"}, EndpointKind::power_output},
            std::pair{std::string_view{"serial"}, EndpointKind::serial},
            std::pair{std::string_view{"contact-input"}, EndpointKind::contact_input},
            std::pair{std::string_view{"contact-output"}, EndpointKind::contact_output},
            std::pair{std::string_view{"analog-current-input"}, EndpointKind::analog_current_input},
            std::pair{std::string_view{"analog-current-output"},
                      EndpointKind::analog_current_output},
            std::pair{std::string_view{"logical"}, EndpointKind::logical},
        };
        static constexpr std::array sensitivities{
            std::pair{std::string_view{"internal"}, Sensitivity::internal},
            std::pair{std::string_view{"restricted"}, Sensitivity::restricted},
        };

        endpoint_domains_.resize(endpoint_statements_.size());
        for (const auto &[external_id, statement] : endpoint_statements_) {
            if (!check_attribute_count(statement->attributes, statement->source)) {
                return false;
            }
            Endpoint value;
            value.id = endpoint_ids_.at(external_id);
            value.external_id = intern(external_id);
            value.source = statement->source;
            const auto owner = resolve_entity(statement->owner, "endpoint owner");
            if (!owner.has_value()) {
                return false;
            }
            value.owner = *owner;
            value.scope = model_.entities[value.owner.value()].scope;
            if (const auto *attribute = find_attribute(statement->attributes, "scope")) {
                if (!expect_identifier(*attribute)) {
                    return false;
                }
                const auto found = scope_ids_.find(attribute->value.text);
                if (found == scope_ids_.end()) {
                    return fail(DiagnosticCode::reference_unresolved, attribute->value.source,
                                "scope '" + attribute->value.text + "' is not declared");
                }
                if (found->second != value.scope) {
                    return fail(DiagnosticCode::layer_incompatible, attribute->value.source,
                                "endpoint scope must match its owner scope");
                }
            }
            if (const auto *attribute = find_attribute(statement->attributes, "kind")) {
                const auto parsed = parse_enum(*attribute, endpoint_kinds);
                if (!parsed.has_value()) {
                    return false;
                }
                value.kind = *parsed;
            }
            const auto owner_plane = model_.entities[value.owner.value()].plane;
            if (value.kind == EndpointKind::network && owner_plane != Plane::communication) {
                return fail(DiagnosticCode::endpoint_incompatible, statement->source,
                            "network endpoint owner must be in the communication plane");
            }
            const bool control_signal_endpoint = value.kind == EndpointKind::serial ||
                                                 value.kind == EndpointKind::contact_input ||
                                                 value.kind == EndpointKind::contact_output ||
                                                 value.kind == EndpointKind::analog_current_input ||
                                                 value.kind == EndpointKind::analog_current_output;
            if (control_signal_endpoint && owner_plane != Plane::control_signal) {
                return fail(DiagnosticCode::endpoint_incompatible, statement->source,
                            "serial and signal endpoint owners must be in the control-signal "
                            "plane");
            }
            if (const auto *attribute = find_attribute(statement->attributes, "label")) {
                value.label = intern(attribute->value.text);
            }
            if (const auto *attribute = find_attribute(statement->attributes, "network-space")) {
                if (!expect_identifier(*attribute)) {
                    return false;
                }
                const auto found = entity_ids_.find(attribute->value.text);
                if (found == entity_ids_.end()) {
                    return fail(DiagnosticCode::reference_unresolved, attribute->value.source,
                                "network-space '" + attribute->value.text + "' is not declared");
                }
                value.network_space = found->second;
            }
            if (const auto *attribute = find_attribute(statement->attributes, "sensitivity")) {
                const auto parsed = parse_enum(*attribute, sensitivities);
                if (!parsed.has_value()) {
                    return false;
                }
                value.sensitivity = *parsed;
            }
            if (const auto *attribute = find_attribute(statement->attributes, "source-ref")) {
                value.source_ref = intern(attribute->value.text);
            }
            if (find_attribute(statement->attributes, "address") != nullptr &&
                !value.network_space.is_valid()) {
                return fail(DiagnosticCode::dsl_missing_required_field, statement->source,
                            "endpoint with address= must name network-space=");
            }
            for (const auto &attribute : statement->attributes) {
                if (attribute.key == "failure-domain") {
                    if (!expect_identifier(attribute)) {
                        return false;
                    }
                    endpoint_domains_[value.id.value()].push_back(
                        {attribute.value.text, attribute.value.source});
                }
            }
            value.attributes = append_attributes(statement->attributes,
                                                 {"kind", "scope", "label", "network-space",
                                                  "failure-domain", "sensitivity", "source-ref"});
            model_.endpoints.push_back(value);
        }
        return true;
    }

    [[nodiscard]] bool validate_network_space(EntityId reference, SourceSpan source,
                                              std::string_view owner) {
        if (!reference.is_valid()) {
            return true;
        }
        const auto external_id = model_.string(model_.entities[reference.value()].external_id);
        if (model_.entities[reference.value()].kind != EntityKind::network_space) {
            return fail(DiagnosticCode::layer_incompatible, source,
                        std::string{owner} + " network-space '" + std::string{external_id} +
                            "' must name a network-space entity");
        }
        return true;
    }

    [[nodiscard]] bool validate_network_spaces() {
        for (const auto &entity : model_.entities) {
            if (!validate_network_space(entity.network_space, entity.source, "entity")) {
                return false;
            }
        }
        for (const auto &endpoint : model_.endpoints) {
            if (!validate_network_space(endpoint.network_space, endpoint.source, "endpoint")) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] FailureDomainKind infer_domain_kind(std::string_view external_id) const noexcept {
        static constexpr std::array values{
            std::pair{std::string_view{"panel"}, FailureDomainKind::panel},
            std::pair{std::string_view{"ups"}, FailureDomainKind::ups},
            std::pair{std::string_view{"cabinet"}, FailureDomainKind::cabinet},
            std::pair{std::string_view{"host"}, FailureDomainKind::host},
            std::pair{std::string_view{"cable-route"}, FailureDomainKind::cable_route},
            std::pair{std::string_view{"carrier"}, FailureDomainKind::carrier},
            std::pair{std::string_view{"antenna"}, FailureDomainKind::antenna},
            std::pair{std::string_view{"route-table"}, FailureDomainKind::route_table},
            std::pair{std::string_view{"cloud-gateway"}, FailureDomainKind::cloud_gateway},
            std::pair{std::string_view{"shared-service"}, FailureDomainKind::shared_service},
        };
        for (const auto &[prefix, kind] : values) {
            if (external_id == prefix ||
                (external_id.size() > prefix.size() &&
                 external_id.substr(0, prefix.size()) == prefix &&
                 (external_id[prefix.size()] == ':' || external_id[prefix.size()] == '-'))) {
                return kind;
            }
        }
        return FailureDomainKind::other;
    }

    [[nodiscard]] bool compile_failure_domains() {
        std::map<std::string, std::vector<std::pair<ObjectRef, SourceSpan>>> memberships;
        auto collect = [&](ObjectRef object, const std::vector<DomainReference> &references) {
            std::map<std::string, SourceSpan> seen;
            for (const auto &reference : references) {
                const auto [existing, inserted] = seen.emplace(reference.id, reference.source);
                if (!inserted) {
                    return fail(DiagnosticCode::id_duplicate, reference.source,
                                "failure-domain '" + reference.id + "' is duplicated on one object",
                                existing->second);
                }
                memberships[reference.id].push_back({object, reference.source});
            }
            return true;
        };
        for (std::size_t index = 0; index < entity_domains_.size(); ++index) {
            if (!collect(ObjectRef::from(EntityId{static_cast<std::uint32_t>(index)}),
                         entity_domains_[index])) {
                return false;
            }
        }
        for (std::size_t index = 0; index < endpoint_domains_.size(); ++index) {
            if (!collect(ObjectRef::from(EndpointId{static_cast<std::uint32_t>(index)}),
                         endpoint_domains_[index])) {
                return false;
            }
        }

        std::map<std::string, FailureDomainId> domain_ids;
        for (const auto &[external_id, members] : memberships) {
            if (!reserve_stable_id(external_id, members.front().second, "failure-domain ID")) {
                return false;
            }
            const auto id =
                FailureDomainId{static_cast<std::uint32_t>(model_.failure_domains.size())};
            domain_ids.emplace(external_id, id);
            model_.failure_domains.push_back(
                {id,
                 intern(external_id),
                 infer_domain_kind(external_id),
                 {},
                 members.empty() ? SourceSpan{} : members.front().second});
        }

        for (std::size_t index = 0; index < entity_domains_.size(); ++index) {
            auto references = entity_domains_[index];
            std::sort(references.begin(), references.end(),
                      [](const auto &left, const auto &right) { return left.id < right.id; });
            const auto begin = static_cast<std::uint32_t>(model_.object_failure_domains.size());
            for (const auto &reference : references) {
                model_.object_failure_domains.push_back(domain_ids.at(reference.id));
            }
            model_.entities[index].failure_domains = {
                begin, static_cast<std::uint32_t>(references.size())};
        }
        for (std::size_t index = 0; index < endpoint_domains_.size(); ++index) {
            auto references = endpoint_domains_[index];
            std::sort(references.begin(), references.end(),
                      [](const auto &left, const auto &right) { return left.id < right.id; });
            const auto begin = static_cast<std::uint32_t>(model_.object_failure_domains.size());
            for (const auto &reference : references) {
                model_.object_failure_domains.push_back(domain_ids.at(reference.id));
            }
            model_.endpoints[index].failure_domains = {
                begin, static_cast<std::uint32_t>(references.size())};
        }

        for (auto &[external_id, members] : memberships) {
            std::sort(members.begin(), members.end(), [&](const auto &left, const auto &right) {
                return object_external_id(left.first) < object_external_id(right.first);
            });
            auto &domain = model_.failure_domains[domain_ids.at(external_id).value()];
            domain.members = {static_cast<std::uint32_t>(model_.failure_domain_members.size()),
                              static_cast<std::uint32_t>(members.size())};
            for (const auto &member : members) {
                model_.failure_domain_members.push_back(member.first);
            }
        }
        return true;
    }

    [[nodiscard]] std::optional<RelationKind> parse_relation_kind(const Attribute &attribute) {
        static constexpr std::array values{
            std::pair{std::string_view{"network"}, RelationKind::network},
            std::pair{std::string_view{"power"}, RelationKind::power},
            std::pair{std::string_view{"serial"}, RelationKind::serial},
            std::pair{std::string_view{"contact"}, RelationKind::contact},
            std::pair{std::string_view{"analog-current"}, RelationKind::analog_current},
            std::pair{std::string_view{"member_of"}, RelationKind::member_of},
            std::pair{std::string_view{"contained_in"}, RelationKind::contained_in},
            std::pair{std::string_view{"carried_by"}, RelationKind::carried_by},
            std::pair{std::string_view{"uses_underlay"}, RelationKind::uses_underlay},
            std::pair{std::string_view{"terminates_on"}, RelationKind::terminates_on},
            std::pair{std::string_view{"routes_via"}, RelationKind::routes_via},
            std::pair{std::string_view{"depends_on"}, RelationKind::depends_on},
            std::pair{std::string_view{"powered_by"}, RelationKind::powered_by},
        };
        return parse_enum(attribute, values);
    }

    [[nodiscard]] bool is_connection_kind(RelationKind kind) const noexcept {
        return relation_kind_index(kind) <= relation_kind_index(RelationKind::analog_current);
    }

    [[nodiscard]] std::string_view object_external_id(ObjectRef object) const noexcept {
        if (object.kind == ObjectKind::entity) {
            return model_.string(model_.entities[object.index].external_id);
        }
        return model_.string(model_.endpoints[object.index].external_id);
    }

    [[nodiscard]] EndpointKind endpoint_kind(ObjectRef object) const noexcept {
        return object.kind == ObjectKind::endpoint ? model_.endpoints[object.index].kind
                                                   : EndpointKind::logical;
    }

    [[nodiscard]] EntityKind entity_kind(ObjectRef object) const noexcept {
        if (object.kind == ObjectKind::entity) {
            return model_.entities[object.index].kind;
        }
        return model_.entities[model_.endpoints[object.index].owner.value()].kind;
    }

    [[nodiscard]] bool endpoint_supports(ObjectRef object, RelationKind relation,
                                         bool source) const noexcept {
        if (object.kind == ObjectKind::entity) {
            const auto kind = entity_kind(object);
            switch (relation) {
            case RelationKind::network:
                return kind == EntityKind::network_space || kind == EntityKind::tunnel;
            case RelationKind::power:
                return source ? kind == EntityKind::facility
                              : kind == EntityKind::device || kind == EntityKind::facility ||
                                    kind == EntityKind::service || kind == EntityKind::process;
            case RelationKind::serial:
                return kind == EntityKind::bus;
            default:
                return false;
            }
        }

        const auto kind = endpoint_kind(object);
        switch (relation) {
        case RelationKind::network:
            return kind == EndpointKind::network;
        case RelationKind::power:
            return source ? kind == EndpointKind::power_output : kind == EndpointKind::power_input;
        case RelationKind::serial:
            return kind == EndpointKind::serial;
        case RelationKind::contact:
            return source ? kind == EndpointKind::contact_output
                          : kind == EndpointKind::contact_input;
        case RelationKind::analog_current:
            return source ? kind == EndpointKind::analog_current_output
                          : kind == EndpointKind::analog_current_input;
        default:
            return false;
        }
    }

    [[nodiscard]] Layer object_layer(ObjectRef object) const noexcept {
        const auto entity = object.kind == ObjectKind::entity
                                ? EntityId{object.index}
                                : model_.endpoints[object.index].owner;
        return model_.entities[entity.value()].layer;
    }

    [[nodiscard]] int layer_rank(Layer layer) const noexcept {
        switch (layer) {
        case Layer::none:
            return -1;
        case Layer::physical:
            return 0;
        case Layer::l2:
            return 1;
        case Layer::l3:
            return 2;
        case Layer::overlay:
            return 3;
        case Layer::virtual_network:
            return 4;
        case Layer::transport:
            return 5;
        case Layer::application:
            return 6;
        }
        return -1;
    }

    [[nodiscard]] bool validate_relation_semantics(const PendingRelation &relation,
                                                   SourceSpan source) {
        if (is_connection_kind(relation.kind)) {
            if ((relation.kind == RelationKind::power || relation.kind == RelationKind::contact ||
                 relation.kind == RelationKind::analog_current) &&
                relation.direction != Direction::directed) {
                return fail(DiagnosticCode::endpoint_incompatible, source,
                            "relation kind '" + std::string{to_string(relation.kind)} +
                                "' must be directed");
            }
            if (!endpoint_supports(relation.from, relation.kind, true) ||
                !endpoint_supports(relation.to, relation.kind, false)) {
                return fail(DiagnosticCode::endpoint_incompatible, source,
                            "relation endpoints are incompatible with kind '" +
                                std::string{to_string(relation.kind)} + "'");
            }
        }
        if (relation.kind == RelationKind::carried_by ||
            relation.kind == RelationKind::uses_underlay) {
            const auto from_rank = layer_rank(object_layer(relation.from));
            const auto to_rank = layer_rank(object_layer(relation.to));
            if (from_rank <= to_rank) {
                return fail(DiagnosticCode::layer_incompatible, source,
                            "relation kind '" + std::string{to_string(relation.kind)} +
                                "' must point from a higher layer to a lower layer");
            }
        }
        if (relation.kind == RelationKind::network &&
            object_layer(relation.from) != object_layer(relation.to)) {
            return fail(DiagnosticCode::layer_incompatible, source,
                        "network connection endpoints must be in the same layer");
        }
        return true;
    }

    [[nodiscard]] bool validate_relation_metadata(const std::vector<Attribute> &attributes) {
        if (const auto *scope = find_attribute(attributes, "scope")) {
            if (!expect_identifier(*scope)) {
                return false;
            }
            if (scope_ids_.find(scope->value.text) == scope_ids_.end()) {
                return fail(DiagnosticCode::reference_unresolved, scope->value.source,
                            "relation scope '" + scope->value.text + "' is not declared");
            }
        }
        if (const auto *sensitivity = find_attribute(attributes, "sensitivity")) {
            if (!expect_identifier(*sensitivity)) {
                return false;
            }
            if (sensitivity->value.text != "internal" && sensitivity->value.text != "restricted") {
                return fail(DiagnosticCode::dsl_invalid_value, sensitivity->value.source,
                            "relation sensitivity must be 'internal' or 'restricted'");
            }
        }
        return true;
    }

    [[nodiscard]] bool compile_relations() {
        std::vector<PendingRelation> pending;
        pending.reserve(document_.statements.size());
        for (const auto &statement : document_.statements) {
            const std::vector<Attribute> *attributes = nullptr;
            const ParsedValue *from_value = nullptr;
            const ParsedValue *to_value = nullptr;
            Direction direction = Direction::directed;
            bool connection = false;
            SourceSpan source;
            if (const auto *connect = std::get_if<dsl::ConnectStatement>(&statement)) {
                attributes = &connect->attributes;
                from_value = &connect->from;
                to_value = &connect->to;
                direction = connect->direction;
                connection = true;
                source = connect->source;
            } else if (const auto *relate = std::get_if<dsl::RelateStatement>(&statement)) {
                attributes = &relate->attributes;
                from_value = &relate->from;
                to_value = &relate->to;
                source = relate->source;
            } else {
                continue;
            }
            if (!check_attribute_count(*attributes, source)) {
                return false;
            }
            const auto *kind_attribute = required_attribute(*attributes, "kind", source);
            if (kind_attribute == nullptr) {
                return false;
            }
            const auto kind = parse_relation_kind(*kind_attribute);
            if (!kind.has_value()) {
                return false;
            }
            if (connection != is_connection_kind(*kind)) {
                return fail(DiagnosticCode::dsl_invalid_value, kind_attribute->value.source,
                            connection ? "connect requires a physical connection kind"
                                       : "relate requires a logical relation kind");
            }
            const auto from = resolve_object(*from_value, "relation source");
            const auto to = resolve_object(*to_value, "relation target");
            if (!from.has_value() || !to.has_value()) {
                return false;
            }
            PendingRelation value{attributes, {}, *from, *to, *kind, direction, source};
            if (direction == Direction::bidirectional &&
                object_external_id(value.to) < object_external_id(value.from)) {
                std::swap(value.from, value.to);
            }
            value.external_id = "relation:" + std::string{to_string(*kind)} + ":" +
                                encoded_id_component(object_external_id(value.from)) + ":" +
                                encoded_id_component(object_external_id(value.to)) + ":" +
                                (direction == Direction::directed ? "d" : "b");
            if (!validate_relation_metadata(*attributes) ||
                !validate_relation_semantics(value, source)) {
                return false;
            }
            pending.push_back(std::move(value));
        }
        std::sort(pending.begin(), pending.end(), [](const auto &left, const auto &right) {
            return left.external_id < right.external_id;
        });
        for (std::size_t index = 1; index < pending.size(); ++index) {
            if (pending[index - 1].external_id == pending[index].external_id) {
                return fail(DiagnosticCode::id_duplicate, pending[index].source,
                            "derived relation ID '" + pending[index].external_id +
                                "' is duplicated",
                            pending[index - 1].source);
            }
        }
        for (const auto &pending_relation : pending) {
            if (!reserve_stable_id(pending_relation.external_id, pending_relation.source,
                                   "relation ID")) {
                return false;
            }
            const auto id = RelationId{static_cast<std::uint32_t>(model_.relations.size())};
            model_.relations.push_back({id, intern(pending_relation.external_id),
                                        pending_relation.from, pending_relation.to,
                                        pending_relation.kind, pending_relation.direction,
                                        append_attributes(*pending_relation.attributes, {"kind"}),
                                        pending_relation.source});
        }
        return true;
    }

    [[nodiscard]] std::optional<std::uint64_t> parse_duration(const Attribute &attribute) {
        if (attribute.value.kind != dsl::ValueKind::duration) {
            fail(DiagnosticCode::dsl_invalid_value, attribute.value.source,
                 "attribute '" + attribute.key + "' must be a duration");
            return std::nullopt;
        }
        const auto text = std::string_view{attribute.value.text};
        const auto unit_at = std::find_if(text.begin(), text.end(),
                                          [](char value) { return !is_ascii_digit(value); });
        const auto digits = text.substr(0, static_cast<std::size_t>(unit_at - text.begin()));
        const auto unit = text.substr(digits.size());
        std::uint64_t value = 0;
        const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), value);
        if (parsed.ec != std::errc{} || parsed.ptr != digits.data() + digits.size()) {
            fail(DiagnosticCode::dsl_invalid_value, attribute.value.source,
                 "duration value is outside the supported range");
            return std::nullopt;
        }
        std::uint64_t multiplier = 0;
        if (unit == "ms") {
            multiplier = 1;
        } else if (unit == "s") {
            multiplier = 1'000;
        } else if (unit == "m") {
            multiplier = 60'000;
        } else if (unit == "h") {
            multiplier = 3'600'000;
        } else if (unit == "d") {
            multiplier = 86'400'000;
        }
        if (multiplier == 0 || value > std::numeric_limits<std::uint64_t>::max() / multiplier) {
            fail(DiagnosticCode::dsl_invalid_value, attribute.value.source,
                 "duration value is outside the supported range");
            return std::nullopt;
        }
        return value * multiplier;
    }

    [[nodiscard]] bool compile_requirements() {
        static constexpr std::array modes{
            std::pair{std::string_view{"all"}, RequirementMode::all},
            std::pair{std::string_view{"any"}, RequirementMode::any},
        };
        std::map<std::string, PendingRequirement> pending;
        for (const auto &statement : document_.statements) {
            const auto *requirement = std::get_if<dsl::RequireStatement>(&statement);
            if (requirement == nullptr) {
                continue;
            }
            if (!check_attribute_count(requirement->attributes, requirement->source)) {
                return false;
            }
            const auto consumer = resolve_entity(requirement->consumer, "requirement consumer");
            const auto provider = resolve_entity(requirement->provider, "requirement provider");
            if (!consumer.has_value() || !provider.has_value()) {
                return false;
            }
            const auto *group_attribute =
                required_attribute(requirement->attributes, "group", requirement->source);
            const auto *mode_attribute =
                required_attribute(requirement->attributes, "mode", requirement->source);
            if (group_attribute == nullptr || mode_attribute == nullptr ||
                !expect_identifier(*group_attribute)) {
                return false;
            }
            const auto mode = parse_enum(*mode_attribute, modes);
            if (!mode.has_value()) {
                return false;
            }
            std::uint64_t hold_up_ms = 0;
            if (const auto *hold_up = find_attribute(requirement->attributes, "hold-up")) {
                const auto duration = parse_duration(*hold_up);
                if (!duration.has_value()) {
                    return false;
                }
                hold_up_ms = *duration;
            }
            const auto consumer_id = object_external_id(ObjectRef::from(*consumer));
            const auto external_id = "requirement:" + encoded_id_component(consumer_id) + ":" +
                                     encoded_id_component(group_attribute->value.text);
            auto [entry, inserted] = pending.emplace(
                external_id,
                PendingRequirement{
                    external_id, *consumer, *mode, hold_up_ms, requirement->source, {}, {}});
            if (!inserted && (entry->second.consumer != *consumer || entry->second.mode != *mode ||
                              entry->second.hold_up_ms != hold_up_ms)) {
                return fail(DiagnosticCode::dsl_invalid_value, requirement->source,
                            "requirement group '" + group_attribute->value.text +
                                "' has inconsistent mode or hold-up",
                            entry->second.source);
            }
            const auto provider_id = std::string{object_external_id(ObjectRef::from(*provider))};
            const auto [existing, provider_inserted] = entry->second.providers.emplace(
                provider_id, std::pair{*provider, requirement->provider.source});
            if (!provider_inserted) {
                return fail(DiagnosticCode::id_duplicate, requirement->provider.source,
                            "provider '" + provider_id + "' is duplicated in one requirement group",
                            existing->second.second);
            }
            for (const auto &attribute : requirement->attributes) {
                if (attribute.extension) {
                    entry->second.extensions.push_back(&attribute);
                }
            }
        }

        for (auto &[external_id, requirement] : pending) {
            if (!reserve_stable_id(external_id, requirement.source, "requirement-group ID")) {
                return false;
            }
            const auto id =
                RequirementGroupId{static_cast<std::uint32_t>(model_.requirement_groups.size())};
            const auto begin = static_cast<std::uint32_t>(model_.requirement_providers.size());
            for (const auto &[provider_id, provider] : requirement.providers) {
                model_.requirement_providers.push_back(provider.first);
                (void)provider_id;
            }
            std::vector<Attribute> extensions;
            for (const auto *extension : requirement.extensions) {
                extensions.push_back(*extension);
            }
            model_.requirement_groups.push_back(
                {id,
                 intern(external_id),
                 requirement.consumer,
                 requirement.mode,
                 {begin, static_cast<std::uint32_t>(requirement.providers.size())},
                 requirement.hold_up_ms,
                 append_attributes(extensions, {}),
                 requirement.source});
        }
        return true;
    }

    [[nodiscard]] std::optional<FailureModeKind> parse_failure_mode(const Attribute &attribute) {
        static constexpr std::array values{
            std::pair{std::string_view{"power-loss"}, FailureModeKind::power_loss},
            std::pair{std::string_view{"breaker-trip"}, FailureModeKind::breaker_trip},
            std::pair{std::string_view{"psu-loss"}, FailureModeKind::psu_loss},
            std::pair{std::string_view{"undervoltage"}, FailureModeKind::undervoltage},
            std::pair{std::string_view{"ups-depleted"}, FailureModeKind::ups_depleted},
            std::pair{std::string_view{"link-down"}, FailureModeKind::link_down},
            std::pair{std::string_view{"port-down"}, FailureModeKind::port_down},
            std::pair{std::string_view{"device-failure"}, FailureModeKind::device_failure},
            std::pair{std::string_view{"route-loss"}, FailureModeKind::route_loss},
            std::pair{std::string_view{"bus-open"}, FailureModeKind::bus_open},
            std::pair{std::string_view{"bus-short"}, FailureModeKind::bus_short},
            std::pair{std::string_view{"termination-fault"}, FailureModeKind::termination_fault},
            std::pair{std::string_view{"master-loss"}, FailureModeKind::master_loss},
            std::pair{std::string_view{"noise"}, FailureModeKind::noise},
            std::pair{std::string_view{"stuck-open"}, FailureModeKind::stuck_open},
            std::pair{std::string_view{"stuck-closed"}, FailureModeKind::stuck_closed},
            std::pair{std::string_view{"wetting-power-loss"}, FailureModeKind::wetting_power_loss},
            std::pair{std::string_view{"logic-inverted"}, FailureModeKind::logic_inverted},
            std::pair{std::string_view{"loop-open"}, FailureModeKind::loop_open},
            std::pair{std::string_view{"under-range"}, FailureModeKind::under_range},
            std::pair{std::string_view{"over-range"}, FailureModeKind::over_range},
            std::pair{std::string_view{"value-frozen"}, FailureModeKind::value_frozen},
            std::pair{std::string_view{"sensor-power-loss"}, FailureModeKind::sensor_power_loss},
            std::pair{std::string_view{"tunnel-down"}, FailureModeKind::tunnel_down},
            std::pair{std::string_view{"peer-unreachable"}, FailureModeKind::peer_unreachable},
            std::pair{std::string_view{"security-association-loss"},
                      FailureModeKind::security_association_loss},
            std::pair{std::string_view{"heartbeat-failed"}, FailureModeKind::heartbeat_failed},
            std::pair{std::string_view{"heartbeat-stale"}, FailureModeKind::heartbeat_stale},
            std::pair{std::string_view{"probe-path-loss"}, FailureModeKind::probe_path_loss},
        };
        if (!expect_identifier(attribute)) {
            return std::nullopt;
        }
        if (attribute.value.text.rfind("x-", 0) == 0 && attribute.value.text.size() > 2) {
            return FailureModeKind::extension;
        }
        return parse_enum(attribute, values);
    }

    [[nodiscard]] bool compile_failures() {
        for (const auto &[external_id, statement] : failure_statements_) {
            if (!check_attribute_count(statement->attributes, statement->source)) {
                return false;
            }
            const auto target = resolve_object(statement->target, "failure target");
            const auto *mode_attribute =
                required_attribute(statement->attributes, "mode", statement->source);
            if (!target.has_value() || mode_attribute == nullptr) {
                return false;
            }
            const auto mode = parse_failure_mode(*mode_attribute);
            if (!mode.has_value()) {
                return false;
            }
            std::uint64_t delay = 0;
            if (const auto *after = find_attribute(statement->attributes, "after")) {
                const auto duration = parse_duration(*after);
                if (!duration.has_value()) {
                    return false;
                }
                delay = *duration;
            }
            const auto id = FailureModeId{static_cast<std::uint32_t>(model_.failure_modes.size())};
            model_.failure_modes.push_back(
                {id, intern(external_id), *target, *mode,
                 *mode == FailureModeKind::extension ? intern(mode_attribute->value.text)
                                                     : StringId{},
                 delay, append_attributes(statement->attributes, {"mode", "after"}),
                 statement->source});
        }
        return true;
    }

    [[nodiscard]] bool compile_probes() {
        static constexpr std::array kinds{
            std::pair{std::string_view{"icmp"}, ProbeKind::icmp},
            std::pair{std::string_view{"tcp-connect"}, ProbeKind::tcp_connect},
            std::pair{std::string_view{"custom-heartbeat"}, ProbeKind::custom_heartbeat},
            std::pair{std::string_view{"metric"}, ProbeKind::metric},
            std::pair{std::string_view{"discrete-state"}, ProbeKind::discrete_state},
        };
        for (const auto &[external_id, statement] : probe_statements_) {
            if (!check_attribute_count(statement->attributes, statement->source)) {
                return false;
            }
            const auto target = resolve_object(statement->target, "probe target");
            const auto vantage = resolve_object(statement->vantage, "probe vantage");
            const auto *kind_attribute =
                required_attribute(statement->attributes, "kind", statement->source);
            const auto *valid_for_attribute =
                required_attribute(statement->attributes, "valid-for", statement->source);
            if (!target.has_value() || !vantage.has_value() || kind_attribute == nullptr ||
                valid_for_attribute == nullptr) {
                return false;
            }
            const auto kind = parse_enum(*kind_attribute, kinds);
            const auto valid_for = parse_duration(*valid_for_attribute);
            if (!kind.has_value() || !valid_for.has_value()) {
                return false;
            }
            const auto id = ProbeId{static_cast<std::uint32_t>(model_.probes.size())};
            model_.probes.push_back(
                {id, intern(external_id), *target, *vantage, *kind, *valid_for,
                 append_attributes(statement->attributes, {"kind", "valid-for"}),
                 statement->source});
        }
        return true;
    }

    [[nodiscard]] std::size_t object_ordinal(ObjectRef object) const noexcept {
        return object.kind == ObjectKind::entity ? object.index
                                                 : model_.entities.size() + object.index;
    }

    [[nodiscard]] ObjectRelationAdjacency &relation_adjacency(ObjectRef object) noexcept {
        return object.kind == ObjectKind::entity ? model_.entity_relation_adjacency[object.index]
                                                 : model_.endpoint_relation_adjacency[object.index];
    }

    [[nodiscard]] bool build_relation_adjacency() {
        using Buckets = std::array<std::vector<RelationAdjacencyEntry>, kRelationKindCount>;
        const auto object_count = model_.entities.size() + model_.endpoints.size();
        std::vector<Buckets> forward(object_count);
        std::vector<Buckets> reverse(object_count);
        for (const auto &relation : model_.relations) {
            const auto kind = relation_kind_index(relation.kind);
            forward[object_ordinal(relation.from)][kind].push_back({relation.to, relation.id});
            reverse[object_ordinal(relation.to)][kind].push_back({relation.from, relation.id});
            if (relation.direction == Direction::bidirectional) {
                forward[object_ordinal(relation.to)][kind].push_back({relation.from, relation.id});
                reverse[object_ordinal(relation.from)][kind].push_back({relation.to, relation.id});
            }
        }
        model_.entity_relation_adjacency.resize(model_.entities.size());
        model_.endpoint_relation_adjacency.resize(model_.endpoints.size());
        const auto compare = [&](const auto &left, const auto &right) {
            const auto left_object = object_external_id(left.object);
            const auto right_object = object_external_id(right.object);
            if (left_object != right_object) {
                return left_object < right_object;
            }
            return model_.string(model_.relations[left.relation.value()].external_id) <
                   model_.string(model_.relations[right.relation.value()].external_id);
        };
        for (std::size_t ordinal = 0; ordinal < object_count; ++ordinal) {
            const auto object =
                ordinal < model_.entities.size()
                    ? ObjectRef::from(EntityId{static_cast<std::uint32_t>(ordinal)})
                    : ObjectRef::from(
                          EndpointId{static_cast<std::uint32_t>(ordinal - model_.entities.size())});
            auto &adjacency = relation_adjacency(object);
            for (std::size_t kind = 0; kind < kRelationKindCount; ++kind) {
                std::sort(forward[ordinal][kind].begin(), forward[ordinal][kind].end(), compare);
                adjacency.forward[kind] = {
                    static_cast<std::uint32_t>(model_.relation_adjacency_entries.size()),
                    static_cast<std::uint32_t>(forward[ordinal][kind].size())};
                model_.relation_adjacency_entries.insert(model_.relation_adjacency_entries.end(),
                                                         forward[ordinal][kind].begin(),
                                                         forward[ordinal][kind].end());
                std::sort(reverse[ordinal][kind].begin(), reverse[ordinal][kind].end(), compare);
                adjacency.reverse[kind] = {
                    static_cast<std::uint32_t>(model_.relation_adjacency_entries.size()),
                    static_cast<std::uint32_t>(reverse[ordinal][kind].size())};
                model_.relation_adjacency_entries.insert(model_.relation_adjacency_entries.end(),
                                                         reverse[ordinal][kind].begin(),
                                                         reverse[ordinal][kind].end());
            }
        }
        return true;
    }

    [[nodiscard]] ObjectAvailabilityAdjacency &availability_adjacency(ObjectRef object) noexcept {
        return object.kind == ObjectKind::entity
                   ? model_.entity_availability_adjacency[object.index]
                   : model_.endpoint_availability_adjacency[object.index];
    }

    [[nodiscard]] bool reject_dependency_cycles() {
        const auto object_count = model_.entities.size() + model_.endpoints.size();
        std::vector<std::uint32_t> indegree(object_count, 0);
        std::vector<std::vector<std::size_t>> outgoing(object_count);
        for (const auto &dependency : model_.availability_dependencies) {
            const auto provider = object_ordinal(dependency.provider);
            const auto consumer = object_ordinal(dependency.consumer);
            ++indegree[consumer];
            outgoing[provider].push_back(consumer);
        }
        std::set<std::size_t> ready;
        for (std::size_t index = 0; index < indegree.size(); ++index) {
            if (indegree[index] == 0) {
                ready.insert(index);
            }
        }
        std::size_t visited = 0;
        while (!ready.empty()) {
            const auto node = *ready.begin();
            ready.erase(ready.begin());
            ++visited;
            for (const auto consumer : outgoing[node]) {
                --indegree[consumer];
                if (indegree[consumer] == 0) {
                    ready.insert(consumer);
                }
            }
        }
        if (visited == object_count) {
            return true;
        }
        for (const auto &dependency : model_.availability_dependencies) {
            if (indegree[object_ordinal(dependency.provider)] != 0 &&
                indegree[object_ordinal(dependency.consumer)] != 0) {
                return fail(DiagnosticCode::dependency_cycle, dependency.source,
                            "availability dependency graph contains a cycle");
            }
        }
        return fail(DiagnosticCode::dependency_cycle, first_source(),
                    "availability dependency graph contains a cycle");
    }

    [[nodiscard]] bool build_availability_dependencies() {
        std::vector<PendingDependency> pending;
        for (const auto &relation : model_.relations) {
            if (relation.kind == RelationKind::power) {
                pending.push_back({relation.from, relation.to, relation.id, {}, relation.source});
            } else if (relation.kind == RelationKind::depends_on ||
                       relation.kind == RelationKind::powered_by ||
                       relation.kind == RelationKind::carried_by ||
                       relation.kind == RelationKind::uses_underlay ||
                       relation.kind == RelationKind::terminates_on ||
                       relation.kind == RelationKind::routes_via) {
                pending.push_back({relation.to, relation.from, relation.id, {}, relation.source});
            }
        }
        for (const auto &requirement : model_.requirement_groups) {
            for (std::uint32_t offset = 0; offset < requirement.providers.count; ++offset) {
                const auto provider =
                    model_.requirement_providers[requirement.providers.begin + offset];
                pending.push_back({ObjectRef::from(provider),
                                   ObjectRef::from(requirement.consumer),
                                   {},
                                   requirement.id,
                                   requirement.source});
            }
        }
        const auto dependency_key = [&](const PendingDependency &dependency) {
            std::string evidence;
            if (dependency.relation.is_valid()) {
                evidence = std::string{
                    model_.string(model_.relations[dependency.relation.value()].external_id)};
            } else {
                evidence = std::string{model_.string(
                    model_.requirement_groups[dependency.requirement_group.value()].external_id)};
            }
            return std::array<std::string, 3>{std::string{object_external_id(dependency.provider)},
                                              std::string{object_external_id(dependency.consumer)},
                                              std::move(evidence)};
        };
        std::sort(pending.begin(), pending.end(), [&](const auto &left, const auto &right) {
            return dependency_key(left) < dependency_key(right);
        });
        for (const auto &dependency : pending) {
            const auto id = AvailabilityDependencyId{
                static_cast<std::uint32_t>(model_.availability_dependencies.size())};
            model_.availability_dependencies.push_back(
                {id, dependency.provider, dependency.consumer, dependency.relation,
                 dependency.requirement_group, dependency.source});
        }
        if (!reject_dependency_cycles()) {
            return false;
        }

        const auto object_count = model_.entities.size() + model_.endpoints.size();
        std::vector<std::vector<AvailabilityAdjacencyEntry>> forward(object_count);
        std::vector<std::vector<AvailabilityAdjacencyEntry>> reverse(object_count);
        for (const auto &dependency : model_.availability_dependencies) {
            forward[object_ordinal(dependency.provider)].push_back(
                {dependency.consumer, dependency.id});
            reverse[object_ordinal(dependency.consumer)].push_back(
                {dependency.provider, dependency.id});
        }
        model_.entity_availability_adjacency.resize(model_.entities.size());
        model_.endpoint_availability_adjacency.resize(model_.endpoints.size());
        const auto compare = [&](const auto &left, const auto &right) {
            const auto left_id = object_external_id(left.object);
            const auto right_id = object_external_id(right.object);
            return left_id != right_id ? left_id < right_id : left.dependency < right.dependency;
        };
        for (std::size_t ordinal = 0; ordinal < object_count; ++ordinal) {
            const auto object =
                ordinal < model_.entities.size()
                    ? ObjectRef::from(EntityId{static_cast<std::uint32_t>(ordinal)})
                    : ObjectRef::from(
                          EndpointId{static_cast<std::uint32_t>(ordinal - model_.entities.size())});
            auto &adjacency = availability_adjacency(object);
            std::sort(forward[ordinal].begin(), forward[ordinal].end(), compare);
            adjacency.forward = {
                static_cast<std::uint32_t>(model_.availability_adjacency_entries.size()),
                static_cast<std::uint32_t>(forward[ordinal].size())};
            model_.availability_adjacency_entries.insert(
                model_.availability_adjacency_entries.end(), forward[ordinal].begin(),
                forward[ordinal].end());
            std::sort(reverse[ordinal].begin(), reverse[ordinal].end(), compare);
            adjacency.reverse = {
                static_cast<std::uint32_t>(model_.availability_adjacency_entries.size()),
                static_cast<std::uint32_t>(reverse[ordinal].size())};
            model_.availability_adjacency_entries.insert(
                model_.availability_adjacency_entries.end(), reverse[ordinal].begin(),
                reverse[ordinal].end());
        }
        return true;
    }

    const dsl::LoadedDocument &document_;
    const ModelLimits &limits_;
    NormalizedModel model_;
    std::optional<Diagnostic> error_;
    std::map<std::string, StringId, std::less<>> interned_strings_;
    std::map<std::string, SourceSpan, std::less<>> stable_ids_;
    std::map<std::string, const dsl::ScopeStatement *> scope_statements_;
    std::map<std::string, const dsl::EntityStatement *> entity_statements_;
    std::map<std::string, const dsl::EndpointStatement *> endpoint_statements_;
    std::map<std::string, const dsl::FailureStatement *> failure_statements_;
    std::map<std::string, const dsl::ProbeStatement *> probe_statements_;
    std::map<std::string, ScopeId> scope_ids_;
    std::map<std::string, EntityId> entity_ids_;
    std::map<std::string, EndpointId> endpoint_ids_;
    std::map<std::string, ObjectRef> object_ids_;
    std::vector<std::vector<DomainReference>> entity_domains_;
    std::vector<std::vector<DomainReference>> endpoint_domains_;
};

} // namespace

CompileResult compile_model(const dsl::LoadedDocument &document, const ModelLimits &limits) {
    return Compiler{document, limits}.run();
}

} // namespace ltfa
