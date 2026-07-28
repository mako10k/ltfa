add_executable(ltfa_smoke_test
    smoke_test.cpp
)
target_link_libraries(ltfa_smoke_test PRIVATE ltfa::core)
target_compile_features(ltfa_smoke_test PRIVATE cxx_std_17)
set_target_properties(ltfa_smoke_test PROPERTIES CXX_EXTENSIONS OFF)
ltfa_enable_warnings(ltfa_smoke_test)

add_test(NAME ltfa.smoke COMMAND ltfa_smoke_test)

add_executable(ltfa_model_contract_test
    model_contract_test.cpp
)
target_link_libraries(ltfa_model_contract_test PRIVATE ltfa::core)
target_compile_features(ltfa_model_contract_test PRIVATE cxx_std_17)
set_target_properties(ltfa_model_contract_test PROPERTIES CXX_EXTENSIONS OFF)
ltfa_enable_warnings(ltfa_model_contract_test)

add_test(NAME ltfa.model.contract COMMAND ltfa_model_contract_test)

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
    PASS_REGULAR_EXPRESSION "Bootstrap scaffold only"
)

add_test(NAME ltfa.cli.version COMMAND $<TARGET_FILE:ltfa> --version)
set_tests_properties(ltfa.cli.version PROPERTIES
    PASS_REGULAR_EXPRESSION "^ltfa 0\\.0\\.0"
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

    if(LTFA_FUZZ_ENGINE STREQUAL "libfuzzer")
        add_library(ltfa_fuzz_core STATIC ${LTFA_CORE_SOURCES})
        target_include_directories(ltfa_fuzz_core
            PUBLIC ${PROJECT_SOURCE_DIR}/include
            PRIVATE ${PROJECT_BINARY_DIR}/generated
        )
        target_compile_features(ltfa_fuzz_core PUBLIC cxx_std_17)
        target_link_libraries(ltfa_fuzz_core PUBLIC ltfa_sanitizers)
        target_compile_options(ltfa_fuzz_core PRIVATE -fsanitize=fuzzer-no-link)
        set_target_properties(ltfa_fuzz_core PROPERTIES CXX_EXTENSIONS OFF)
        ltfa_enable_warnings(ltfa_fuzz_core)

        target_compile_definitions(ltfa_parser_fuzz PRIVATE LTFA_LIBFUZZER=1)
        target_compile_options(ltfa_parser_fuzz PRIVATE -fsanitize=fuzzer)
        target_link_options(ltfa_parser_fuzz PRIVATE -fsanitize=fuzzer)
        target_link_libraries(ltfa_parser_fuzz PRIVATE ltfa_fuzz_core)
    else()
        target_link_libraries(ltfa_parser_fuzz PRIVATE ltfa::core)
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
    endif()
endif()
