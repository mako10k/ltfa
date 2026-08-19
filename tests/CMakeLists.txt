add_executable(ltfa_smoke_test
    smoke_test.cpp
)
target_link_libraries(ltfa_smoke_test PRIVATE ltfa::core)
target_compile_features(ltfa_smoke_test PRIVATE cxx_std_17)
set_target_properties(ltfa_smoke_test PROPERTIES CXX_EXTENSIONS OFF)
ltfa_enable_warnings(ltfa_smoke_test)

add_test(NAME ltfa.smoke COMMAND ltfa_smoke_test)

add_executable(ltfa_yyjson_vendor_test
    yyjson_vendor_test.cpp
)
target_link_libraries(ltfa_yyjson_vendor_test PRIVATE ltfa::yyjson)
target_compile_features(ltfa_yyjson_vendor_test PRIVATE cxx_std_17)
set_target_properties(ltfa_yyjson_vendor_test PROPERTIES CXX_EXTENSIONS OFF)
ltfa_enable_warnings(ltfa_yyjson_vendor_test)

add_test(NAME ltfa.third-party.yyjson COMMAND ltfa_yyjson_vendor_test)

add_executable(ltfa_observation_decoder_test
    observation_decoder_test.cpp
)
target_link_libraries(ltfa_observation_decoder_test PRIVATE ltfa::core)
target_compile_features(ltfa_observation_decoder_test PRIVATE cxx_std_17)
set_target_properties(ltfa_observation_decoder_test PROPERTIES CXX_EXTENSIONS OFF)
ltfa_enable_warnings(ltfa_observation_decoder_test)

add_test(NAME ltfa.observation.decoder COMMAND ltfa_observation_decoder_test)

add_executable(ltfa_evidence_index_test
    evidence_index_test.cpp
)
target_link_libraries(ltfa_evidence_index_test PRIVATE ltfa::core)
target_compile_features(ltfa_evidence_index_test PRIVATE cxx_std_17)
set_target_properties(ltfa_evidence_index_test PROPERTIES CXX_EXTENSIONS OFF)
ltfa_enable_warnings(ltfa_evidence_index_test)

add_test(NAME ltfa.observation.evidence-index COMMAND ltfa_evidence_index_test)

add_executable(ltfa_diagnosis_test
    diagnosis_test.cpp
)
target_link_libraries(ltfa_diagnosis_test PRIVATE ltfa::core)
target_compile_features(ltfa_diagnosis_test PRIVATE cxx_std_17)
set_target_properties(ltfa_diagnosis_test PROPERTIES CXX_EXTENSIONS OFF)
ltfa_enable_warnings(ltfa_diagnosis_test)

add_test(NAME ltfa.analysis.diagnosis COMMAND ltfa_diagnosis_test)

add_executable(ltfa_observation_recommendation_test
    observation_recommendation_test.cpp
)
target_link_libraries(ltfa_observation_recommendation_test PRIVATE ltfa::core)
target_compile_features(ltfa_observation_recommendation_test PRIVATE cxx_std_17)
set_target_properties(ltfa_observation_recommendation_test PROPERTIES CXX_EXTENSIONS OFF)
ltfa_enable_warnings(ltfa_observation_recommendation_test)

add_test(NAME ltfa.analysis.next-observation COMMAND ltfa_observation_recommendation_test)

add_executable(ltfa_redacted_history_acceptance_test
    redacted_history_acceptance_test.cpp
)
target_link_libraries(ltfa_redacted_history_acceptance_test PRIVATE ltfa::core)
target_compile_features(ltfa_redacted_history_acceptance_test PRIVATE cxx_std_17)
target_compile_definitions(ltfa_redacted_history_acceptance_test PRIVATE
    LTFA_REDACTED_FIXTURE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/redacted-history"
)
set_target_properties(ltfa_redacted_history_acceptance_test PROPERTIES CXX_EXTENSIONS OFF)
ltfa_enable_warnings(ltfa_redacted_history_acceptance_test)

add_test(NAME ltfa.acceptance.redacted-history COMMAND ltfa_redacted_history_acceptance_test)

add_executable(ltfa_slice3_acceptance_test
    slice3_acceptance_test.cpp
)
target_link_libraries(ltfa_slice3_acceptance_test PRIVATE ltfa::core ltfa::yyjson)
target_compile_features(ltfa_slice3_acceptance_test PRIVATE cxx_std_17)
target_compile_definitions(ltfa_slice3_acceptance_test PRIVATE
    LTFA_REDACTED_FIXTURE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/redacted-history"
)
set_target_properties(ltfa_slice3_acceptance_test PROPERTIES CXX_EXTENSIONS OFF)
ltfa_enable_warnings(ltfa_slice3_acceptance_test)

add_test(NAME ltfa.acceptance.slice3 COMMAND ltfa_slice3_acceptance_test)

add_executable(ltfa_model_contract_test
    model_contract_test.cpp
)
target_link_libraries(ltfa_model_contract_test PRIVATE ltfa::core)
target_compile_features(ltfa_model_contract_test PRIVATE cxx_std_17)
set_target_properties(ltfa_model_contract_test PROPERTIES CXX_EXTENSIONS OFF)
ltfa_enable_warnings(ltfa_model_contract_test)

add_test(NAME ltfa.model.contract COMMAND ltfa_model_contract_test)

add_executable(ltfa_availability_test
    availability_test.cpp
)
target_link_libraries(ltfa_availability_test PRIVATE ltfa::core)
target_compile_features(ltfa_availability_test PRIVATE cxx_std_17)
set_target_properties(ltfa_availability_test PROPERTIES CXX_EXTENSIONS OFF)
ltfa_enable_warnings(ltfa_availability_test)

add_test(NAME ltfa.assurance.requirements COMMAND ltfa_availability_test)

add_executable(ltfa_path_analysis_test
    path_analysis_test.cpp
)
target_link_libraries(ltfa_path_analysis_test PRIVATE ltfa::core)
target_compile_features(ltfa_path_analysis_test PRIVATE cxx_std_17)
set_target_properties(ltfa_path_analysis_test PROPERTIES CXX_EXTENSIONS OFF)
ltfa_enable_warnings(ltfa_path_analysis_test)

add_test(NAME ltfa.analysis.path-impact COMMAND ltfa_path_analysis_test)

add_executable(ltfa_cut_analysis_test
    cut_analysis_test.cpp
)
target_link_libraries(ltfa_cut_analysis_test PRIVATE ltfa::core)
target_compile_features(ltfa_cut_analysis_test PRIVATE cxx_std_17)
set_target_properties(ltfa_cut_analysis_test PROPERTIES CXX_EXTENSIONS OFF)
ltfa_enable_warnings(ltfa_cut_analysis_test)

add_test(NAME ltfa.analysis.service-cuts COMMAND ltfa_cut_analysis_test)

add_executable(ltfa_failure_propagation_test
    failure_propagation_test.cpp
)
target_link_libraries(ltfa_failure_propagation_test PRIVATE ltfa::core)
target_compile_features(ltfa_failure_propagation_test PRIVATE cxx_std_17)
set_target_properties(ltfa_failure_propagation_test PROPERTIES CXX_EXTENSIONS OFF)
ltfa_enable_warnings(ltfa_failure_propagation_test)

add_test(NAME ltfa.analysis.failure-propagation COMMAND ltfa_failure_propagation_test)

add_executable(ltfa_spof_analysis_test
    spof_analysis_test.cpp
)
target_link_libraries(ltfa_spof_analysis_test PRIVATE ltfa::core)
target_compile_features(ltfa_spof_analysis_test PRIVATE cxx_std_17)
set_target_properties(ltfa_spof_analysis_test PROPERTIES CXX_EXTENSIONS OFF)
ltfa_enable_warnings(ltfa_spof_analysis_test)

add_test(NAME ltfa.analysis.spof COMMAND ltfa_spof_analysis_test)

add_executable(ltfa_slice2_acceptance_test
    slice2_acceptance_test.cpp
)
target_link_libraries(ltfa_slice2_acceptance_test PRIVATE ltfa::core)
target_compile_features(ltfa_slice2_acceptance_test PRIVATE cxx_std_17)
target_compile_definitions(ltfa_slice2_acceptance_test PRIVATE
    LTFA_SLICE2_FIXTURE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/slice2"
)
set_target_properties(ltfa_slice2_acceptance_test PROPERTIES CXX_EXTENSIONS OFF)
ltfa_enable_warnings(ltfa_slice2_acceptance_test)

add_test(NAME ltfa.acceptance.slice2 COMMAND ltfa_slice2_acceptance_test)

add_executable(ltfa_lexer_test
    lexer_test.cpp
)
target_link_libraries(ltfa_lexer_test PRIVATE ltfa::core)
target_compile_features(ltfa_lexer_test PRIVATE cxx_std_17)
set_target_properties(ltfa_lexer_test PROPERTIES CXX_EXTENSIONS OFF)
ltfa_enable_warnings(ltfa_lexer_test)

add_test(NAME ltfa.dsl.lexer COMMAND ltfa_lexer_test)

add_executable(ltfa_parser_test
    parser_test.cpp
)
target_link_libraries(ltfa_parser_test PRIVATE ltfa::core)
target_compile_features(ltfa_parser_test PRIVATE cxx_std_17)
set_target_properties(ltfa_parser_test PROPERTIES CXX_EXTENSIONS OFF)
ltfa_enable_warnings(ltfa_parser_test)

add_test(NAME ltfa.dsl.parser COMMAND ltfa_parser_test)

add_executable(ltfa_include_resolver_test
    include_resolver_test.cpp
)
target_link_libraries(ltfa_include_resolver_test PRIVATE ltfa::core)
target_compile_features(ltfa_include_resolver_test PRIVATE cxx_std_17)
set_target_properties(ltfa_include_resolver_test PROPERTIES CXX_EXTENSIONS OFF)
ltfa_enable_warnings(ltfa_include_resolver_test)

add_test(NAME ltfa.dsl.includes COMMAND ltfa_include_resolver_test)

add_executable(ltfa_model_compiler_test
    model_compiler_test.cpp
)
target_link_libraries(ltfa_model_compiler_test PRIVATE ltfa::core)
target_compile_features(ltfa_model_compiler_test PRIVATE cxx_std_17)
set_target_properties(ltfa_model_compiler_test PROPERTIES CXX_EXTENSIONS OFF)
ltfa_enable_warnings(ltfa_model_compiler_test)

add_test(NAME ltfa.model.compiler COMMAND ltfa_model_compiler_test)

add_executable(ltfa_model_output_test
    model_output_test.cpp
)
target_link_libraries(ltfa_model_output_test PRIVATE ltfa::core)
target_compile_features(ltfa_model_output_test PRIVATE cxx_std_17)
set_target_properties(ltfa_model_output_test PROPERTIES CXX_EXTENSIONS OFF)
ltfa_enable_warnings(ltfa_model_output_test)

add_test(NAME ltfa.model.output COMMAND ltfa_model_output_test)

add_executable(ltfa_parser_hardening_test
    parser_hardening_test.cpp
)
target_link_libraries(ltfa_parser_hardening_test PRIVATE ltfa::core)
target_compile_features(ltfa_parser_hardening_test PRIVATE cxx_std_17)
set_target_properties(ltfa_parser_hardening_test PROPERTIES CXX_EXTENSIONS OFF)
ltfa_enable_warnings(ltfa_parser_hardening_test)

add_test(NAME ltfa.dsl.hardening COMMAND ltfa_parser_hardening_test)

add_test(NAME ltfa.cli.help COMMAND $<TARGET_FILE:ltfa> --help)
set_tests_properties(ltfa.cli.help PROPERTIES
    PASS_REGULAR_EXPRESSION "ltfa model --model-root"
)

add_test(NAME ltfa.cli.version COMMAND $<TARGET_FILE:ltfa> --version)
string(REPLACE "." "[.]" LTFA_PROJECT_VERSION_REGEX "${LTFA_VERSION_STRING}")
set_tests_properties(ltfa.cli.version PROPERTIES
    PASS_REGULAR_EXPRESSION "^ltfa ${LTFA_PROJECT_VERSION_REGEX}"
)

add_test(
    NAME ltfa.cli.acceptance
    COMMAND ${CMAKE_COMMAND}
        -DLTFA_CLI=$<TARGET_FILE:ltfa>
        -DLTFA_EXPECTED_VERSION=${LTFA_VERSION_STRING}
        -DLTFA_VALID_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/slice1
        -DLTFA_INVALID_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/slice1-invalid
        -DLTFA_SLICE2_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/slice2
        -DLTFA_RESILIENCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/resilience
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_acceptance_test.cmake
)

add_test(
    NAME ltfa.cli.analyze.budget-acceptance
    COMMAND ${CMAKE_COMMAND}
        -DLTFA_CLI=$<TARGET_FILE:ltfa>
        -DLTFA_SLICE2_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/slice2
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_budget_acceptance_test.cmake
)

add_test(
    NAME ltfa.cli.assurance-output-parity
    COMMAND ${CMAKE_COMMAND}
        -DLTFA_CLI=$<TARGET_FILE:ltfa>
        -DLTFA_SLICE2_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/slice2
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cli_assurance_output_parity_test.cmake
)

add_test(
    NAME ltfa.install.smoke
    COMMAND ${CMAKE_COMMAND}
        -DLTFA_BUILD_DIR=${PROJECT_BINARY_DIR}
        -DLTFA_INSTALL_PREFIX=${PROJECT_BINARY_DIR}/install-smoke
        -DLTFA_INSTALL_BINDIR=${CMAKE_INSTALL_BINDIR}
        -DLTFA_INSTALL_DOCDIR=${CMAKE_INSTALL_DOCDIR}
        -DLTFA_EXECUTABLE_SUFFIX=${CMAKE_EXECUTABLE_SUFFIX}
        -DLTFA_INSTALL_CONFIG=$<CONFIG>
        -DLTFA_EXPECTED_VERSION=${LTFA_VERSION_STRING}
        -DLTFA_MODEL_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/slice1
        -DLTFA_SLICE2_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/slice2
        -P ${PROJECT_SOURCE_DIR}/cmake/verify_install_smoke.cmake
)

add_test(
    NAME ltfa.boundary.offline
    COMMAND ${CMAKE_COMMAND}
        -DLTFA_SOURCE_DIR=${PROJECT_SOURCE_DIR}
        -DLTFA_CORE_ARCHIVE=$<TARGET_FILE:ltfa_core>
        -DLTFA_CLI=$<TARGET_FILE:ltfa>
        -DLTFA_NM=${CMAKE_NM}
        -P ${PROJECT_SOURCE_DIR}/cmake/verify_offline_boundary.cmake
)

if(NOT LTFA_SANITIZERS AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_program(LTFA_STRACE_EXECUTABLE strace)
    if(LTFA_STRACE_EXECUTABLE)
        add_test(
            NAME ltfa.boundary.offline-runtime
            COMMAND ${CMAKE_COMMAND}
                -DLTFA_STRACE=${LTFA_STRACE_EXECUTABLE}
                -DLTFA_PROGRAM=$<TARGET_FILE:ltfa_parser_hardening_test>
                -DLTFA_TRACE=${CMAKE_CURRENT_BINARY_DIR}/ltfa-offline-runtime.trace
                -P ${PROJECT_SOURCE_DIR}/cmake/verify_offline_runtime.cmake
        )
        add_test(
            NAME ltfa.boundary.cli-offline-runtime
            COMMAND ${CMAKE_COMMAND}
                -DLTFA_STRACE=${LTFA_STRACE_EXECUTABLE}
                -DLTFA_CLI=$<TARGET_FILE:ltfa>
                -DLTFA_MODEL_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/slice1
                -DLTFA_SLICE2_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/slice2
                -DLTFA_TRACE=${CMAKE_CURRENT_BINARY_DIR}/ltfa-cli-offline-runtime.trace
                -P ${PROJECT_SOURCE_DIR}/cmake/verify_offline_cli_runtime.cmake
        )
        add_test(
            NAME ltfa.boundary.slice2-offline-runtime
            COMMAND ${CMAKE_COMMAND}
                -DLTFA_STRACE=${LTFA_STRACE_EXECUTABLE}
                -DLTFA_PROGRAM=$<TARGET_FILE:ltfa_slice2_acceptance_test>
                -DLTFA_TRACE=${CMAKE_CURRENT_BINARY_DIR}/ltfa-slice2-offline-runtime.trace
                -P ${PROJECT_SOURCE_DIR}/cmake/verify_offline_runtime.cmake
        )
        add_test(
            NAME ltfa.boundary.observation-offline-runtime
            COMMAND ${CMAKE_COMMAND}
                -DLTFA_STRACE=${LTFA_STRACE_EXECUTABLE}
                -DLTFA_PROGRAM=$<TARGET_FILE:ltfa_observation_decoder_test>
                -DLTFA_TRACE=${CMAKE_CURRENT_BINARY_DIR}/ltfa-observation-offline-runtime.trace
                -P ${PROJECT_SOURCE_DIR}/cmake/verify_offline_runtime.cmake
        )
        add_test(
            NAME ltfa.boundary.evidence-index-offline-runtime
            COMMAND ${CMAKE_COMMAND}
                -DLTFA_STRACE=${LTFA_STRACE_EXECUTABLE}
                -DLTFA_PROGRAM=$<TARGET_FILE:ltfa_evidence_index_test>
                -DLTFA_TRACE=${CMAKE_CURRENT_BINARY_DIR}/ltfa-evidence-index-offline-runtime.trace
                -P ${PROJECT_SOURCE_DIR}/cmake/verify_offline_runtime.cmake
        )
        add_test(
            NAME ltfa.boundary.diagnosis-offline-runtime
            COMMAND ${CMAKE_COMMAND}
                -DLTFA_STRACE=${LTFA_STRACE_EXECUTABLE}
                -DLTFA_PROGRAM=$<TARGET_FILE:ltfa_diagnosis_test>
                -DLTFA_TRACE=${CMAKE_CURRENT_BINARY_DIR}/ltfa-diagnosis-offline-runtime.trace
                -P ${PROJECT_SOURCE_DIR}/cmake/verify_offline_runtime.cmake
        )
        add_test(
            NAME ltfa.boundary.redacted-history-offline-runtime
            COMMAND ${CMAKE_COMMAND}
                -DLTFA_STRACE=${LTFA_STRACE_EXECUTABLE}
                -DLTFA_PROGRAM=$<TARGET_FILE:ltfa_redacted_history_acceptance_test>
                -DLTFA_TRACE=${CMAKE_CURRENT_BINARY_DIR}/ltfa-redacted-history-offline-runtime.trace
                -P ${PROJECT_SOURCE_DIR}/cmake/verify_offline_runtime.cmake
        )
        add_test(
            NAME ltfa.boundary.slice3-offline-runtime
            COMMAND ${CMAKE_COMMAND}
                -DLTFA_STRACE=${LTFA_STRACE_EXECUTABLE}
                -DLTFA_PROGRAM=$<TARGET_FILE:ltfa_slice3_acceptance_test>
                -DLTFA_TRACE=${CMAKE_CURRENT_BINARY_DIR}/ltfa-slice3-offline-runtime.trace
                -P ${PROJECT_SOURCE_DIR}/cmake/verify_offline_runtime.cmake
        )
    endif()
endif()

find_program(LTFA_DOT_EXECUTABLE dot)
if(LTFA_DOT_EXECUTABLE)
    add_test(
        NAME ltfa.model.output.graphviz
        COMMAND ${CMAKE_COMMAND}
            -DLTFA_DOT=${LTFA_DOT_EXECUTABLE}
            -DLTFA_DOT_PRODUCER=$<TARGET_FILE:ltfa_model_output_test>
            -DLTFA_DOT_FILE=${CMAKE_CURRENT_BINARY_DIR}/ltfa-model-output.dot
            -DLTFA_SVG_FILE=${CMAKE_CURRENT_BINARY_DIR}/ltfa-model-output.svg
            -P ${PROJECT_SOURCE_DIR}/cmake/verify_dot_render.cmake
    )
    add_test(
        NAME ltfa.model.output.semantic-graphviz
        COMMAND ${CMAKE_COMMAND}
            -DLTFA_DOT=${LTFA_DOT_EXECUTABLE}
            -DLTFA_DOT_PRODUCER=$<TARGET_FILE:ltfa_model_output_test>
            -DLTFA_DOT_PRODUCER_ARG=--emit-semantic-dot
            -DLTFA_DOT_FILE=${CMAKE_CURRENT_BINARY_DIR}/ltfa-semantic-output.dot
            -DLTFA_SVG_FILE=${CMAKE_CURRENT_BINARY_DIR}/ltfa-semantic-output.svg
            -P ${PROJECT_SOURCE_DIR}/cmake/verify_dot_render.cmake
    )
    add_test(
        NAME ltfa.analysis.diagnosis-output-graphviz
        COMMAND ${CMAKE_COMMAND}
            -DLTFA_DOT=${LTFA_DOT_EXECUTABLE}
            -DLTFA_DOT_PRODUCER=$<TARGET_FILE:ltfa_slice3_acceptance_test>
            -DLTFA_DOT_PRODUCER_ARG=--emit-dot
            -DLTFA_DOT_FILE=${CMAKE_CURRENT_BINARY_DIR}/ltfa-diagnosis-output.dot
            -DLTFA_SVG_FILE=${CMAKE_CURRENT_BINARY_DIR}/ltfa-diagnosis-output.svg
            -P ${PROJECT_SOURCE_DIR}/cmake/verify_dot_render.cmake
    )
    foreach(view logical protection power dependency overlay)
        add_test(
            NAME ltfa.model.output.semantic-${view}-graphviz
            COMMAND ${CMAKE_COMMAND}
                -DLTFA_DOT=${LTFA_DOT_EXECUTABLE}
                -DLTFA_DOT_PRODUCER=$<TARGET_FILE:ltfa_model_output_test>
                -DLTFA_DOT_PRODUCER_ARG=--emit-semantic-${view}-dot
                -DLTFA_DOT_FILE=${CMAKE_CURRENT_BINARY_DIR}/ltfa-semantic-${view}-output.dot
                -DLTFA_SVG_FILE=${CMAKE_CURRENT_BINARY_DIR}/ltfa-semantic-${view}-output.svg
                -P ${PROJECT_SOURCE_DIR}/cmake/verify_dot_render.cmake
        )
    endforeach()
endif()

if(LTFA_BUILD_FUZZERS)
    if(NOT LTFA_FUZZ_ENGINE MATCHES "^(standalone|libfuzzer)$")
        message(FATAL_ERROR
            "Unsupported LTFA_FUZZ_ENGINE '${LTFA_FUZZ_ENGINE}'; use standalone or libfuzzer"
        )
    endif()
    if(LTFA_FUZZ_ENGINE STREQUAL "libfuzzer" AND
       NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "LTFA libFuzzer targets require Clang")
    endif()

    add_executable(ltfa_parser_fuzz
        fuzz/parser_fuzz.cpp
    )
    target_include_directories(ltfa_parser_fuzz PRIVATE ${PROJECT_SOURCE_DIR}/include)
    target_compile_features(ltfa_parser_fuzz PRIVATE cxx_std_17)
    set_target_properties(ltfa_parser_fuzz PROPERTIES CXX_EXTENSIONS OFF)
    ltfa_enable_warnings(ltfa_parser_fuzz)

    add_executable(ltfa_observation_fuzz
        fuzz/observation_fuzz.cpp
    )
    target_include_directories(ltfa_observation_fuzz PRIVATE ${PROJECT_SOURCE_DIR}/include)
    target_compile_features(ltfa_observation_fuzz PRIVATE cxx_std_17)
    set_target_properties(ltfa_observation_fuzz PROPERTIES CXX_EXTENSIONS OFF)
    ltfa_enable_warnings(ltfa_observation_fuzz)

    if(LTFA_FUZZ_ENGINE STREQUAL "libfuzzer")
        add_library(ltfa_fuzz_core STATIC ${LTFA_CORE_SOURCES})
        target_include_directories(ltfa_fuzz_core
            PUBLIC ${PROJECT_SOURCE_DIR}/include
            PRIVATE ${PROJECT_BINARY_DIR}/generated
        )
        target_compile_features(ltfa_fuzz_core PUBLIC cxx_std_17)
        target_link_libraries(ltfa_fuzz_core
            PUBLIC ltfa_sanitizers
            PRIVATE ltfa::yyjson
        )
        target_compile_options(ltfa_fuzz_core PRIVATE -fsanitize=fuzzer-no-link)
        set_target_properties(ltfa_fuzz_core PROPERTIES CXX_EXTENSIONS OFF)
        ltfa_enable_warnings(ltfa_fuzz_core)

        target_compile_definitions(ltfa_parser_fuzz PRIVATE LTFA_LIBFUZZER=1)
        target_compile_options(ltfa_parser_fuzz PRIVATE -fsanitize=fuzzer)
        target_link_options(ltfa_parser_fuzz PRIVATE -fsanitize=fuzzer)
        target_link_libraries(ltfa_parser_fuzz PRIVATE ltfa_fuzz_core)

        target_compile_definitions(ltfa_observation_fuzz PRIVATE LTFA_LIBFUZZER=1)
        target_compile_options(ltfa_observation_fuzz PRIVATE -fsanitize=fuzzer)
        target_link_options(ltfa_observation_fuzz PRIVATE -fsanitize=fuzzer)
        target_link_libraries(ltfa_observation_fuzz PRIVATE ltfa_fuzz_core)
    else()
        target_link_libraries(ltfa_parser_fuzz PRIVATE ltfa::core)
        target_link_libraries(ltfa_observation_fuzz PRIVATE ltfa::core)
    endif()

    if(LTFA_FUZZ_ENGINE STREQUAL "standalone")
        add_test(
            NAME ltfa.fuzz.parser.seeds
            COMMAND ltfa_parser_fuzz
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parser/minimal.ltfa
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parser/unterminated-string.ltfa
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parser/dependency-cycle.ltfa
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parser/include-uri.ltfa
        )
        add_test(
            NAME ltfa.fuzz.observation.seeds
            COMMAND ltfa_observation_fuzz
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/observation/valid.jsonl
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/observation/two-records.jsonl
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/observation/trailing-comma.jsonl
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/observation/future-major.jsonl
        )
    endif()
endif()
