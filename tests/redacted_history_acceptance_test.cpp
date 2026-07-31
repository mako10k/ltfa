#include "ltfa/diagnosis.hpp"
#include "ltfa/dsl/include_resolver.hpp"
#include "ltfa/model_compiler.hpp"
#include "ltfa/observation.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifndef LTFA_REDACTED_FIXTURE_ROOT
#error "LTFA_REDACTED_FIXTURE_ROOT must name the redacted fixture directory"
#endif

namespace {

constexpr ltfa::StringId kObservationFile{61};

void expect(bool condition, std::string_view message) {
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

std::string read_file(const std::filesystem::path &path) {
    std::ifstream stream{path, std::ios::binary};
    expect(stream.is_open(), "redacted fixture file must open");
    return {std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
}

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

bool contains_ipv4_address(std::string_view value) {
    for (std::size_t start = 0; start < value.size(); ++start) {
        if (!std::isdigit(static_cast<unsigned char>(value[start])) ||
            (start != 0 && std::isdigit(static_cast<unsigned char>(value[start - 1])))) {
            continue;
        }
        auto position = start;
        bool valid = true;
        for (int part = 0; part < 4; ++part) {
            unsigned int number = 0;
            std::size_t digits = 0;
            while (position < value.size() &&
                   std::isdigit(static_cast<unsigned char>(value[position])) && digits < 3) {
                number = number * 10U + static_cast<unsigned int>(value[position] - '0');
                ++position;
                ++digits;
            }
            if (digits == 0 || number > 255 ||
                (position < value.size() &&
                 std::isdigit(static_cast<unsigned char>(value[position])))) {
                valid = false;
                break;
            }
            if (part != 3) {
                if (position >= value.size() || value[position] != '.') {
                    valid = false;
                    break;
                }
                ++position;
            }
        }
        if (valid && (position == value.size() ||
                      !std::isdigit(static_cast<unsigned char>(value[position])))) {
            return true;
        }
    }
    return false;
}

void test_redaction_surface(const std::filesystem::path &root) {
    expect(contains_ipv4_address("address=192.0.2.1") &&
               !contains_ipv4_address("date=2030-01-01") &&
               !contains_ipv4_address("invalid=999.0.2.1"),
           "redaction IPv4 scanner must distinguish addresses from synthetic dates");
    const auto payload = read_file(root / "main.ltfa") + read_file(root / "observations.jsonl");
    const auto lowered = lowercase(payload);
    constexpr std::string_view forbidden[]{
        "ami-a",        "site_ami",     "site-ip",     "site_ip",         "target-ip",
        "target_ip",    "hostname",     "database",    "dbname",          "se-converter",
        "se_converter", "alarm-label",  "alarm_label", "current-reading", "current_reading",
        "office",       "redmine",      "journal",     "source-file",     "source_file",
        "evidence-ref", "evidence_ref", "producer-id", "producer_id",     "detail",
        "password",     "passwd",       "credential",  "secret",          "token",
        "phone",        "username",     "user-name",
    };
    for (const auto term : forbidden) {
        expect(lowered.find(term) == std::string::npos,
               "fixture must not retain an operational or secret-shaped field");
    }

    expect(!contains_ipv4_address(payload), "fixture must not contain an IPv4 address");
    expect(payload.find('@') == std::string::npos, "fixture must not contain an email address");
    for (const auto prefix : {"/home/", "/etc/", "/var/", "/opt/", "/srv/"}) {
        expect(lowered.find(prefix) == std::string::npos,
               "fixture must not contain an operational absolute path");
    }
}

ltfa::NormalizedModel compile_fixture(const std::filesystem::path &root) {
    const auto loaded = ltfa::dsl::load_document_tree(root, "main.ltfa");
    if (loaded.error.has_value()) {
        std::cerr << ltfa::to_string(loaded.error->code) << ": " << loaded.error->message << '\n';
    }
    expect(loaded.ok(), "redacted historical fixture must load");
    const auto compiled = ltfa::compile_model(*loaded.document);
    if (compiled.error.has_value()) {
        std::cerr << ltfa::to_string(compiled.error->code) << ": " << compiled.error->message
                  << '\n';
    }
    expect(compiled.ok(), "redacted historical fixture must compile");
    return *compiled.model;
}

std::vector<std::string> mapping_facts(const ltfa::NormalizedModel &model) {
    std::vector<std::string> facts;
    for (const auto &entity : model.entities) {
        const auto external_id = model.string(entity.external_id);
        if (external_id.find("service:collector-") != 0) {
            continue;
        }
        expect(entity.attributes.count == 1, "every synthetic mapping must retain one count");
        const auto &attribute = model.attributes[entity.attributes.begin];
        expect(model.string(attribute.key) == "x-device-count" &&
                   attribute.kind == ltfa::AttributeValueKind::integer,
               "mapping fanout must use one typed synthetic count");
        facts.push_back(std::string{external_id} + ":" +
                        std::string{model.string(attribute.value)});
    }
    std::sort(facts.begin(), facts.end());
    return facts;
}

ltfa::EvidenceIndex build_evidence(const ltfa::NormalizedModel &model,
                                   const std::filesystem::path &root) {
    const auto decoded = ltfa::decode_observation_json_lines(read_file(root / "observations.jsonl"),
                                                             kObservationFile);
    if (decoded.error.has_value()) {
        std::cerr << ltfa::to_string(decoded.error->code) << ": " << decoded.error->message << '\n';
    }
    expect(decoded.ok() && decoded.observations.size() == 6,
           "redacted fixture must retain exactly six synthetic observations");
    for (const auto &observation : decoded.observations) {
        expect(!observation.evidence_ref.has_value() && !observation.detail.has_value() &&
                   !observation.producer_id.has_value() &&
                   !observation.producer_sequence.has_value() && observation.extensions.empty(),
               "synthetic observations must not retain raw evidence or producer detail");
    }

    const auto reference = ltfa::parse_observation_timestamp("2030-01-01T00:10:30Z");
    expect(reference.ok(), "synthetic analysis reference time must parse");
    const auto indexed =
        ltfa::build_evidence_index(model, decoded.observations, *reference.timestamp);
    if (indexed.error.has_value()) {
        std::cerr << ltfa::to_string(indexed.error->code) << ": " << indexed.error->message << '\n';
    }
    expect(indexed.ok(), "redacted observations must build an evidence index");
    return *indexed.index;
}

std::vector<std::string> diagnosis_facts(const ltfa::DiagnosisResult &result) {
    std::vector<std::string> facts;
    for (const auto &finding : result.findings) {
        facts.push_back(finding.hypothesis.stable_id + ":" + std::to_string(finding.rank) + ":" +
                        (finding.tied ? "tied" : "distinct") + ":" +
                        std::to_string(finding.contradictions) + ":" +
                        std::to_string(finding.explained_non_pass) + ":" +
                        std::to_string(finding.unexplained_non_pass) + ":" +
                        std::string{ltfa::to_string(finding.qualification.completeness)} + ":" +
                        std::string{ltfa::to_string(finding.qualification.evidence_quality)});
    }
    return facts;
}

void test_structure_and_diagnosis(const std::filesystem::path &root) {
    const auto model = compile_fixture(root);
    expect(model.scopes.size() == 2 && model.entities.size() == 12 && model.endpoints.empty() &&
               model.relations.size() == 9 && model.failure_modes.size() == 4 &&
               model.probes.size() == 4,
           "fixture must retain only the documented synthetic structural dimensions");
    expect(mapping_facts(model) ==
               std::vector<std::string>{"service:collector-01:11", "service:collector-02:2",
                                        "service:collector-03:11", "service:collector-04:2",
                                        "service:collector-05:9"},
           "five mapping fanouts must preserve only the approved structural counts");

    const auto evidence = build_evidence(model, root);
    expect(evidence.groups.size() == 4 && evidence.conflicts.size() == 2,
           "fixture must retain one same-probe and one cross-vantage conflict");
    const auto same_group =
        std::count_if(evidence.conflicts.begin(), evidence.conflicts.end(),
                      [](const ltfa::EvidenceConflict &value) {
                          return value.kind == ltfa::EvidenceConflictKind::same_probe_and_vantage;
                      });
    const auto across_vantages =
        std::count_if(evidence.conflicts.begin(), evidence.conflicts.end(),
                      [](const ltfa::EvidenceConflict &value) {
                          return value.kind == ltfa::EvidenceConflictKind::across_vantages;
                      });
    expect(same_group == 1 && across_vantages == 1,
           "both synthetic conflict classes must remain explicit");

    const auto first = ltfa::diagnose_observations(model, evidence);
    const auto second = ltfa::diagnose_observations(model, evidence);
    expect(first.status == ltfa::DiagnosisStatus::complete && first.findings.size() == 10,
           "redacted fixture must produce the bounded explainable candidate set");
    expect(first.findings.front().hypothesis.stable_id == "failure-mode:failure:remote-path" &&
               first.findings.front().rank == 1 && !first.findings.front().tied &&
               first.findings.front().contradictions == 0 &&
               first.findings.front().explained_non_pass == 1 &&
               first.findings.front().unexplained_non_pass == 1,
           "remote path loss must outrank hypotheses contradicted by a current site pass");
    expect(first.findings.front().conflicting_evidence.size() == 4 &&
               first.findings.front().remaining_unknown_evidence.size() == 2 &&
               first.findings.front().qualification.completeness == ltfa::Completeness::partial &&
               first.findings.front().qualification.evidence_quality ==
                   ltfa::EvidenceQuality::inferred,
           "best finding must retain conflicts, unknowns, and partial/inferred qualification");
    expect(diagnosis_facts(first) == diagnosis_facts(second) &&
               first.usage.causal_objects == second.usage.causal_objects &&
               first.usage.candidate_hypotheses == second.usage.candidate_hypotheses &&
               first.usage.simulations == second.usage.simulations &&
               first.usage.evidence_comparisons == second.usage.evidence_comparisons,
           "identical fixture analysis must retain stable order, ranks, and usage");
    expect(std::none_of(first.findings.begin(), first.findings.end(),
                        [](const ltfa::DiagnosisFinding &finding) {
                            return finding.hypothesis.stable_id.find("service:collector-") !=
                                   std::string::npos;
                        }),
           "downstream inventory mappings must not escape the observation causal cone");
}

} // namespace

int main() {
    const std::filesystem::path root{LTFA_REDACTED_FIXTURE_ROOT};
    test_redaction_surface(root);
    test_structure_and_diagnosis(root);
    return 0;
}
