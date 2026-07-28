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

add_test(NAME ltfa.cli.help COMMAND $<TARGET_FILE:ltfa> --help)
set_tests_properties(ltfa.cli.help PROPERTIES
    PASS_REGULAR_EXPRESSION "Bootstrap scaffold only"
)

add_test(NAME ltfa.cli.version COMMAND $<TARGET_FILE:ltfa> --version)
set_tests_properties(ltfa.cli.version PROPERTIES
    PASS_REGULAR_EXPRESSION "^ltfa 0\\.0\\.0"
)
