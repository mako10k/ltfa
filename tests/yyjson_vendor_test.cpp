#include <yyjson.h>

#include <cstring>
#include <iostream>
#include <string>
#include <string_view>

#if !defined(YYJSON_DISABLE_WRITER) || YYJSON_DISABLE_WRITER != 1
#error "LTFA must compile yyjson without the writer"
#endif

#if !defined(YYJSON_DISABLE_INCR_READER) || YYJSON_DISABLE_INCR_READER != 1
#error "LTFA must compile yyjson without the incremental reader"
#endif

#if !defined(YYJSON_DISABLE_UTILS) || YYJSON_DISABLE_UTILS != 1
#error "LTFA must compile yyjson without JSON Pointer/Patch utilities"
#endif

#if !defined(YYJSON_DISABLE_NON_STANDARD) || YYJSON_DISABLE_NON_STANDARD != 1
#error "LTFA must compile yyjson without non-standard JSON extensions"
#endif

namespace {

bool rejects(std::string_view input) {
    std::string mutable_input(input);
    yyjson_read_err error{};
    yyjson_doc *document = yyjson_read_opts(mutable_input.data(), mutable_input.size(),
                                            YYJSON_READ_NOFLAG, nullptr, &error);
    if (document != nullptr) {
        yyjson_doc_free(document);
        return false;
    }
    return error.code != YYJSON_READ_SUCCESS;
}

} // namespace

int main() {
    if (std::string_view(YYJSON_VERSION_STRING) != "0.12.0") {
        std::cerr << "unexpected yyjson version: " << YYJSON_VERSION_STRING << '\n';
        return 1;
    }

    constexpr std::string_view valid =
        R"({"schema":"ltfa.observation/v1","label":"えびの","valid_for_ms":60000})";
    std::string mutable_valid(valid);
    yyjson_read_err error{};
    yyjson_doc *document = yyjson_read_opts(mutable_valid.data(), mutable_valid.size(),
                                            YYJSON_READ_NOFLAG, nullptr, &error);
    if (document == nullptr) {
        std::cerr << "strict valid JSON was rejected at byte " << error.pos << ": "
                  << (error.msg == nullptr ? "unknown error" : error.msg) << '\n';
        return 1;
    }

    yyjson_val *root = yyjson_doc_get_root(document);
    yyjson_val *schema = yyjson_obj_get(root, "schema");
    const char *schema_text = yyjson_get_str(schema);
    const bool valid_schema =
        schema_text != nullptr && std::strcmp(schema_text, "ltfa.observation/v1") == 0;
    yyjson_doc_free(document);
    if (!valid_schema) {
        std::cerr << "valid JSON did not preserve the schema string\n";
        return 1;
    }

    constexpr char invalid_utf8[] = "{\"label\":\"\xC3\x28\"}";
    if (!rejects(R"({"value":1,})") || !rejects(R"({"value":NaN})") ||
        !rejects("/* comment */ {}") ||
        !rejects(std::string_view(invalid_utf8, sizeof(invalid_utf8) - 1U)) || !rejects("{} {}")) {
        std::cerr << "strict yyjson defaults accepted a forbidden input\n";
        return 1;
    }

    return 0;
}
