#include "ltfa/dsl/include_resolver.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

namespace {

namespace fs = std::filesystem;

int failures = 0;

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << message << '\n';
        ++failures;
    }
}

class TemporaryWorkspace {
  public:
    TemporaryWorkspace() {
        static std::uint64_t sequence = 0;
        const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
        for (;;) {
            base_ = fs::temp_directory_path() /
                    ("ltfa-includes-" + std::to_string(tick) + "-" + std::to_string(sequence++));
            std::error_code error;
            if (fs::create_directory(base_, error)) {
                break;
            }
            if (error) {
                std::cerr << "cannot create temporary test directory\n";
                ++failures;
                break;
            }
        }
        root_ = base_ / "root";
        std::error_code error;
        fs::create_directories(root_, error);
        if (error) {
            std::cerr << "cannot create temporary model root\n";
            ++failures;
        }
    }

    TemporaryWorkspace(const TemporaryWorkspace &) = delete;
    TemporaryWorkspace &operator=(const TemporaryWorkspace &) = delete;

    ~TemporaryWorkspace() {
        std::error_code ignored;
        fs::remove_all(base_, ignored);
    }

    [[nodiscard]] const fs::path &base() const noexcept { return base_; }
    [[nodiscard]] const fs::path &root() const noexcept { return root_; }

    void write(const fs::path &path, std::string_view contents) const {
        std::error_code error;
        fs::create_directories(path.parent_path(), error);
        if (error) {
            std::cerr << "cannot create fixture directory\n";
            ++failures;
            return;
        }
        std::ofstream output{path, std::ios::binary};
        output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        if (!output) {
            std::cerr << "cannot write fixture file\n";
            ++failures;
        }
    }

  private:
    fs::path base_;
    fs::path root_;
};

const ltfa::Diagnostic *expect_error(const ltfa::dsl::LoadResult &result,
                                     ltfa::DiagnosticCode code) {
    expect(!result.document.has_value(), "failed loading must not expose partial statements");
    if (!result.error.has_value()) {
        std::cerr << "expected diagnostic " << ltfa::to_string(code) << '\n';
        ++failures;
        return nullptr;
    }
    if (result.error->code != code) {
        std::cerr << "expected diagnostic " << ltfa::to_string(code) << ", got "
                  << ltfa::to_string(result.error->code) << '\n';
        ++failures;
    }
    return &*result.error;
}

void test_nested_include_expansion() {
    TemporaryWorkspace workspace;
    const std::string main = "ltfa 1\n"
                             "scope before\n"
                             "include \"sub/one.ltfa\"\n"
                             "scope after\n";
    const std::string one = "ltfa 1\n"
                            "entity device:one kind=device scope=before\n"
                            "include \"../common.ltfa\"\n";
    const std::string common = "ltfa 1\n"
                               "endpoint endpoint:one owner=device:one kind=network\n";
    workspace.write(workspace.root() / "main.ltfa", main);
    workspace.write(workspace.root() / "sub/one.ltfa", one);
    workspace.write(workspace.root() / "common.ltfa", common);

    const auto result = ltfa::dsl::load_document_tree(workspace.root(), fs::path{"main.ltfa"});
    expect(result.ok(), "nested local includes under the model root must load");
    if (!result.ok()) {
        if (result.error.has_value()) {
            std::cerr << ltfa::to_string(result.error->code) << ": " << result.error->message
                      << '\n';
        }
        return;
    }

    expect(result.sources.size() == 3, "each canonical source must receive one file ID");
    expect(result.document->statements.size() == 4,
           "include statements must be replaced by depth-first contents");
    expect(result.document->total_source_bytes == main.size() + one.size() + common.size(),
           "total source bytes must include the root and every included file");
    expect(result.document->canonical_model_root == fs::canonical(workspace.root()),
           "the selected model root must be canonical");

    const auto *before = std::get_if<ltfa::dsl::ScopeStatement>(&result.document->statements[0]);
    const auto *entity = std::get_if<ltfa::dsl::EntityStatement>(&result.document->statements[1]);
    const auto *endpoint =
        std::get_if<ltfa::dsl::EndpointStatement>(&result.document->statements[2]);
    const auto *after = std::get_if<ltfa::dsl::ScopeStatement>(&result.document->statements[3]);
    expect(before != nullptr && before->id.text == "before", "root prefix must remain first");
    expect(entity != nullptr && entity->source.file == ltfa::StringId{1},
           "first included file must retain its source ID");
    expect(endpoint != nullptr && endpoint->source.file == ltfa::StringId{2},
           "nested file must retain its source ID");
    expect(after != nullptr && after->id.text == "after", "root suffix must remain last");
    for (const auto &source : result.sources) {
        expect(source.header_source.has_value(), "loaded sources must retain header spans");
        expect(source.canonical_path.is_absolute(), "source table paths must be canonical");
    }
}

void test_duplicate_noncyclic_include_is_textual() {
    TemporaryWorkspace workspace;
    const std::string main = "ltfa 1\n"
                             "include \"child.ltfa\"\n"
                             "include \"child.ltfa\"\n";
    const std::string child = "ltfa 1\nscope duplicated\n";
    workspace.write(workspace.root() / "main.ltfa", main);
    workspace.write(workspace.root() / "child.ltfa", child);

    const auto result = ltfa::dsl::load_document_tree(workspace.root(), "main.ltfa");
    expect(result.ok(), "the same file may be included again after its stack frame closes");
    if (!result.ok()) {
        return;
    }
    expect(result.sources.size() == 2, "canonical source table must deduplicate file paths");
    expect(result.document->statements.size() == 2,
           "repeated includes must retain textual expansion semantics");
    expect(result.document->total_source_bytes == main.size() + child.size() * 2,
           "total byte budget must count every include expansion");
}

void test_lexical_and_symlink_escape_rejection() {
    TemporaryWorkspace workspace;
    workspace.write(workspace.base() / "outside.ltfa", "ltfa 1\nscope outside\n");
    workspace.write(workspace.root() / "main.ltfa", "ltfa 1\ninclude \"../outside.ltfa\"\n");

    auto result = ltfa::dsl::load_document_tree(workspace.root(), "main.ltfa");
    const auto *escape = expect_error(result, ltfa::DiagnosticCode::include_escape);
    if (escape != nullptr) {
        expect(escape->source.begin.line == 2, "escape diagnostic must point at the include path");
    }

    workspace.write(workspace.root() / "main.ltfa", "ltfa 1\ninclude \"../does-not-exist.ltfa\"\n");
    result = ltfa::dsl::load_document_tree(workspace.root(), "main.ltfa");
    expect_error(result, ltfa::DiagnosticCode::include_escape);

    const auto outside_directory = workspace.base() / "outside";
    workspace.write(outside_directory / "secret.ltfa", "ltfa 1\nscope outside\n");
    std::error_code symlink_error;
    fs::create_directory_symlink(outside_directory, workspace.root() / "link", symlink_error);
    if (!symlink_error) {
        workspace.write(workspace.root() / "main.ltfa", "ltfa 1\ninclude \"link/secret.ltfa\"\n");
        result = ltfa::dsl::load_document_tree(workspace.root(), "main.ltfa");
        expect_error(result, ltfa::DiagnosticCode::include_escape);
    }

    result = ltfa::dsl::load_document_tree(workspace.root(), workspace.base() / "outside.ltfa");
    expect_error(result, ltfa::DiagnosticCode::include_escape);
}

void test_nonlocal_and_expanding_paths_are_rejected() {
    TemporaryWorkspace workspace;
    workspace.write(workspace.root() / "main.ltfa",
                    "ltfa 1\ninclude \"https://example.invalid/model.ltfa\"\n");
    auto result = ltfa::dsl::load_document_tree(workspace.root(), "main.ltfa");
    expect_error(result, ltfa::DiagnosticCode::include_invalid);

    workspace.write(workspace.root() / "main.ltfa", "ltfa 1\ninclude \"$MODEL_ROOT/child.ltfa\"\n");
    result = ltfa::dsl::load_document_tree(workspace.root(), "main.ltfa");
    expect_error(result, ltfa::DiagnosticCode::include_invalid);

    const auto absolute = (workspace.root() / "child.ltfa").generic_string();
    workspace.write(workspace.root() / "child.ltfa", "ltfa 1\nscope child\n");
    workspace.write(workspace.root() / "main.ltfa", "ltfa 1\ninclude \"" + absolute + "\"\n");
    result = ltfa::dsl::load_document_tree(workspace.root(), "main.ltfa");
    expect_error(result, ltfa::DiagnosticCode::include_escape);
}

void test_cycles_and_depth_limit() {
    TemporaryWorkspace workspace;
    workspace.write(workspace.root() / "main.ltfa", "ltfa 1\ninclude \"a.ltfa\"\n");
    workspace.write(workspace.root() / "a.ltfa", "ltfa 1\ninclude \"main.ltfa\"\n");

    auto result = ltfa::dsl::load_document_tree(workspace.root(), "main.ltfa");
    const auto *cycle = expect_error(result, ltfa::DiagnosticCode::include_cycle);
    if (cycle != nullptr) {
        expect(cycle->secondary_source.has_value(),
               "cycle diagnostic must identify the active source header");
        expect(cycle->source.file == ltfa::StringId{1},
               "cycle diagnostic must point at the closing include");
    }

    workspace.write(workspace.root() / "a.ltfa", "ltfa 1\ninclude \"b.ltfa\"\n");
    workspace.write(workspace.root() / "b.ltfa", "ltfa 1\nscope too-deep\n");
    auto limits = ltfa::kDefaultModelLimits;
    limits.max_include_depth = 1;
    result = ltfa::dsl::load_document_tree(workspace.root(), "main.ltfa", limits);
    expect_error(result, ltfa::DiagnosticCode::include_depth);
}

void test_file_and_total_byte_limits() {
    TemporaryWorkspace workspace;
    const std::string main = "ltfa 1\ninclude \"child.ltfa\"\n";
    const std::string child = "ltfa 1\nscope child label=\"this file is deliberately long\"\n";
    workspace.write(workspace.root() / "main.ltfa", main);
    workspace.write(workspace.root() / "child.ltfa", child);

    auto limits = ltfa::kDefaultModelLimits;
    limits.max_source_file_bytes = main.size() + 1;
    auto result = ltfa::dsl::load_document_tree(workspace.root(), "main.ltfa", limits);
    expect_error(result, ltfa::DiagnosticCode::limit_source_file_bytes);

    limits = ltfa::kDefaultModelLimits;
    limits.max_total_source_bytes = main.size() + child.size() - 1;
    result = ltfa::dsl::load_document_tree(workspace.root(), "main.ltfa", limits);
    expect_error(result, ltfa::DiagnosticCode::limit_total_source_bytes);
}

void test_multi_scope_defaults_must_be_explicit() {
    TemporaryWorkspace workspace;
    workspace.write(workspace.root() / "main.ltfa", "ltfa 1\n"
                                                    "scope site:a\n"
                                                    "include \"site-b.ltfa\"\n"
                                                    "entity device:router kind=device\n");
    workspace.write(workspace.root() / "site-b.ltfa", "ltfa 1\nscope site:b\n");

    auto result = ltfa::dsl::load_document_tree(workspace.root(), "main.ltfa");
    const auto *ambiguous = expect_error(result, ltfa::DiagnosticCode::dsl_ambiguous_scope);
    if (ambiguous != nullptr) {
        expect(ambiguous->source.begin.line == 4,
               "ambiguous scope diagnostic must point at the affected entity");
    }

    workspace.write(workspace.root() / "main.ltfa",
                    "ltfa 1\n"
                    "scope site:a\n"
                    "include \"site-b.ltfa\"\n"
                    "entity device:router kind=device scope=site:a\n");
    result = ltfa::dsl::load_document_tree(workspace.root(), "main.ltfa");
    expect(result.ok(), "explicit entity scopes must disambiguate a multi-scope model");
}

void test_io_and_nested_parse_diagnostics() {
    TemporaryWorkspace workspace;
    workspace.write(workspace.root() / "main.ltfa", "ltfa 1\ninclude \"missing.ltfa\"\n");
    auto result = ltfa::dsl::load_document_tree(workspace.root(), "main.ltfa");
    expect_error(result, ltfa::DiagnosticCode::include_io);

    fs::create_directory(workspace.root() / "directory.ltfa");
    workspace.write(workspace.root() / "main.ltfa", "ltfa 1\ninclude \"directory.ltfa\"\n");
    result = ltfa::dsl::load_document_tree(workspace.root(), "main.ltfa");
    expect_error(result, ltfa::DiagnosticCode::include_io);

    workspace.write(workspace.root() / "bad.ltfa", "ltfa 2\n");
    workspace.write(workspace.root() / "main.ltfa", "ltfa 1\ninclude \"bad.ltfa\"\n");
    result = ltfa::dsl::load_document_tree(workspace.root(), "main.ltfa");
    const auto *parse_error = expect_error(result, ltfa::DiagnosticCode::dsl_invalid_header);
    expect(result.sources.size() == 2,
           "source table must remain available for nested parse diagnostics");
    if (parse_error != nullptr) {
        expect(parse_error->source.file == ltfa::StringId{1},
               "nested parser diagnostic must identify the included file");
    }
}

void test_invalid_model_root() {
    TemporaryWorkspace workspace;
    const auto result =
        ltfa::dsl::load_document_tree(workspace.base() / "missing-root", "main.ltfa");
    expect_error(result, ltfa::DiagnosticCode::include_io);
}

} // namespace

int main() {
    test_nested_include_expansion();
    test_duplicate_noncyclic_include_is_textual();
    test_lexical_and_symlink_escape_rejection();
    test_nonlocal_and_expanding_paths_are_rejected();
    test_cycles_and_depth_limit();
    test_file_and_total_byte_limits();
    test_multi_scope_defaults_must_be_explicit();
    test_io_and_nested_parse_diagnostics();
    test_invalid_model_root();
    return failures == 0 ? 0 : 1;
}
