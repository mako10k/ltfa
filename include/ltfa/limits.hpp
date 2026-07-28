#pragma once

#include <cstddef>
#include <cstdint>

namespace ltfa {

struct ModelLimits {
    std::uint64_t max_source_file_bytes{16U * 1024U * 1024U};
    std::uint64_t max_total_source_bytes{64U * 1024U * 1024U};
    std::size_t max_line_bytes{8U * 1024U};
    std::uint32_t max_include_depth{16};
    std::uint32_t max_entities{100'000};
    std::uint32_t max_relations{500'000};
    std::uint32_t max_attributes_per_object{64};
    std::size_t max_attribute_string_bytes{4U * 1024U};
    std::size_t max_observation_line_bytes{64U * 1024U};
};

inline constexpr ModelLimits kDefaultModelLimits{};

} // namespace ltfa
