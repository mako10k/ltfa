#pragma once

#include <cstdint>
#include <limits>

namespace ltfa {

template <typename Tag> class IndexId {
  public:
    using Value = std::uint32_t;

    constexpr IndexId() noexcept = default;
    explicit constexpr IndexId(Value value) noexcept : value_(value) {}

    [[nodiscard]] constexpr Value value() const noexcept { return value_; }

    [[nodiscard]] constexpr bool is_valid() const noexcept { return value_ != invalid_value(); }

    [[nodiscard]] static constexpr Value invalid_value() noexcept {
        return std::numeric_limits<Value>::max();
    }

  private:
    Value value_{invalid_value()};
};

template <typename Tag>
[[nodiscard]] constexpr bool operator==(IndexId<Tag> left, IndexId<Tag> right) noexcept {
    return left.value() == right.value();
}

template <typename Tag>
[[nodiscard]] constexpr bool operator!=(IndexId<Tag> left, IndexId<Tag> right) noexcept {
    return !(left == right);
}

template <typename Tag>
[[nodiscard]] constexpr bool operator<(IndexId<Tag> left, IndexId<Tag> right) noexcept {
    return left.value() < right.value();
}

using StringId = IndexId<struct StringIdTag>;
using ScopeId = IndexId<struct ScopeIdTag>;
using EntityId = IndexId<struct EntityIdTag>;
using EndpointId = IndexId<struct EndpointIdTag>;
using RelationId = IndexId<struct RelationIdTag>;
using RequirementGroupId = IndexId<struct RequirementGroupIdTag>;
using FailureDomainId = IndexId<struct FailureDomainIdTag>;
using FailureModeId = IndexId<struct FailureModeIdTag>;
using ProbeId = IndexId<struct ProbeIdTag>;

struct IndexRange {
    std::uint32_t begin{0};
    std::uint32_t count{0};
};

} // namespace ltfa
