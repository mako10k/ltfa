#pragma once

#include <cstddef>
#include <iosfwd>
#include <string>
#include <string_view>

namespace ltfa::app {

enum class GuideLevel {
    index,
    quick,
    detail,
};

struct GuidePageDescriptor {
    std::string_view topic;
    std::string_view subtopic;
    std::string_view title;
    std::string_view summary;
    std::string_view quick_reference;
    std::string_view syntax;
    std::string_view notes;
    std::string_view examples;
    std::string_view related;
};

struct GuideCatalog {
    const GuidePageDescriptor *pages;
    std::size_t page_count;
};

struct ObservationGuideDescriptor {
    std::string_view schema;
    std::string_view framing;
    std::string_view required_fields;
    std::string_view optional_fields;
    std::string_view kinds;
    std::string_view outcomes;
    std::string_view guide_invocation;
    std::string_view error_guide_invocation;
    std::string_view reference_path;
    std::string_view sample_path;
};

[[nodiscard]] GuideCatalog guide_catalog();
[[nodiscard]] const ObservationGuideDescriptor &observation_guide_descriptor();
[[nodiscard]] const GuidePageDescriptor *find_guide_page(std::string_view topic,
                                                         std::string_view subtopic = {});
[[nodiscard]] std::string suggest_guide_topic(std::string_view topic);
[[nodiscard]] std::string suggest_guide_subtopic(std::string_view topic, std::string_view subtopic);
[[nodiscard]] std::string_view to_string(GuideLevel level) noexcept;
[[nodiscard]] GuideLevel default_guide_level(std::string_view topic, std::string_view subtopic);

void print_language_guide_text(std::ostream &output, const GuidePageDescriptor *page,
                               GuideLevel level);
void print_language_guide_json(std::ostream &output, const GuidePageDescriptor *page,
                               GuideLevel level);

} // namespace ltfa::app
