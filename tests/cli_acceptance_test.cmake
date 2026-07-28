foreach(required LTFA_CLI LTFA_VALID_ROOT LTFA_INVALID_ROOT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "missing required setting ${required}")
    endif()
endforeach()

function(run_cli prefix)
    execute_process(
        COMMAND "${LTFA_CLI}" ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    set("${prefix}_result" "${result}" PARENT_SCOPE)
    set("${prefix}_output" "${output}" PARENT_SCOPE)
    set("${prefix}_error" "${error}" PARENT_SCOPE)
endfunction()

set(model_command
    model
    --model-root "${LTFA_VALID_ROOT}"
    --input main.ltfa
)

run_cli(json_first ${model_command} --format json)
run_cli(json_second ${model_command} --format json)
if(NOT json_first_result EQUAL 0 OR NOT json_second_result EQUAL 0)
    message(FATAL_ERROR "JSON model command failed: ${json_first_error}${json_second_error}")
endif()
if(NOT json_first_error STREQUAL "" OR NOT json_second_error STREQUAL "")
    message(FATAL_ERROR "successful JSON model command wrote stderr")
endif()
if(NOT json_first_output STREQUAL json_second_output)
    message(FATAL_ERROR "JSON model output changed between identical runs")
endif()

string(JSON schema GET "${json_first_output}" schema)
string(JSON operation GET "${json_first_output}" operation)
string(JSON entity_count GET "${json_first_output}" model counts entities)
string(JSON endpoint_count GET "${json_first_output}" model counts endpoints)
string(JSON relation_count GET "${json_first_output}" model counts relations)
string(JSON model_input_count LENGTH "${json_first_output}" model_inputs)
if(NOT schema STREQUAL "ltfa.model/v1" OR NOT operation STREQUAL "model-summary")
    message(FATAL_ERROR "JSON envelope schema or operation is incorrect")
endif()
if(NOT entity_count EQUAL 15 OR NOT endpoint_count EQUAL 14 OR NOT relation_count EQUAL 15)
    message(FATAL_ERROR
        "acceptance fixture counts changed: entities=${entity_count}, "
        "endpoints=${endpoint_count}, relations=${relation_count}"
    )
endif()
if(NOT model_input_count EQUAL 5)
    message(FATAL_ERROR "JSON must identify all five loaded topology inputs")
endif()

set(expected_inputs
    communication.ltfa
    control.ltfa
    facility.ltfa
    main.ltfa
    service.ltfa
)
set(input_index 0)
foreach(expected IN LISTS expected_inputs)
    string(JSON actual GET "${json_first_output}" model_inputs ${input_index})
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "unexpected model input ${input_index}: ${actual}")
    endif()
    math(EXPR input_index "${input_index} + 1")
endforeach()

run_cli(human ${model_command} --format human)
if(NOT human_result EQUAL 0 OR NOT human_error STREQUAL "" OR
   NOT human_output MATCHES "^result: complete")
    message(FATAL_ERROR "human model command did not return a complete result")
endif()

run_cli(dot_first ${model_command} --format dot)
run_cli(dot_second ${model_command} --format dot)
if(NOT dot_first_result EQUAL 0 OR NOT dot_second_result EQUAL 0 OR
   NOT dot_first_error STREQUAL "" OR NOT dot_second_error STREQUAL "")
    message(FATAL_ERROR "DOT model command failed")
endif()
if(NOT dot_first_output STREQUAL dot_second_output OR
   NOT dot_first_output MATCHES "^digraph ltfa_model")
    message(FATAL_ERROR "DOT model output must be valid-looking and deterministic")
endif()

run_cli(restricted_default
    model --model-root "${LTFA_VALID_ROOT}" --input restricted.ltfa --format json
)
run_cli(restricted_explicit
    model --model-root "${LTFA_VALID_ROOT}" --input restricted.ltfa --format json
    --include-restricted
)
string(FIND "${restricted_default_output}" "\"id\":\"restricted:device\"" default_restricted)
string(FIND "${restricted_explicit_output}" "\"id\":\"restricted:device\"" explicit_restricted)
if(NOT restricted_default_result EQUAL 0 OR NOT restricted_explicit_result EQUAL 0 OR
   NOT default_restricted EQUAL -1 OR explicit_restricted EQUAL -1)
    message(FATAL_ERROR "restricted CLI output must require the explicit inclusion flag")
endif()

run_cli(malformed
    model --model-root "${LTFA_INVALID_ROOT}" --input malformed.ltfa --format json
)
if(NOT malformed_result EQUAL 3 OR NOT malformed_output STREQUAL "" OR
   NOT malformed_error MATCHES
       "^malformed.ltfa:2:1: error E_DSL_UNKNOWN_STATEMENT: unknown DSL statement")
    message(FATAL_ERROR "malformed input must return exit 3 with a stable source diagnostic")
endif()

run_cli(duplicate
    model --model-root "${LTFA_INVALID_ROOT}" --input duplicate.ltfa --format json
)
if(NOT duplicate_result EQUAL 4 OR NOT duplicate_output STREQUAL "" OR
   NOT duplicate_error MATCHES
       "^duplicate.ltfa:4:8: error E_ID_DUPLICATE: entity ID 'duplicate' is duplicated")
    message(FATAL_ERROR "validation failure must return exit 4 with a stable source diagnostic")
endif()

run_cli(escape
    model --model-root "${LTFA_INVALID_ROOT}/escape-root" --input main.ltfa --format json
)
if(NOT escape_result EQUAL 3 OR NOT escape_output STREQUAL "" OR
   NOT escape_error MATCHES
       "^main.ltfa:2:9: error E_INCLUDE_ESCAPE: include path escapes the selected model root")
    message(FATAL_ERROR "escaping include must fail as an input error")
endif()

run_cli(usage
    model --model-root "${LTFA_VALID_ROOT}" --input main.ltfa --format yaml
)
if(NOT usage_result EQUAL 2 OR NOT usage_output STREQUAL "" OR
   NOT usage_error MATCHES "^ltfa: --format must be human, json, or dot")
    message(FATAL_ERROR "invalid CLI syntax must return exit 2")
endif()

run_cli(absolute_input
    model --model-root "${LTFA_VALID_ROOT}" --input "${LTFA_VALID_ROOT}/main.ltfa"
)
if(NOT absolute_input_result EQUAL 2 OR NOT absolute_input_output STREQUAL "" OR
   NOT absolute_input_error MATCHES "^ltfa: --input must be relative to --model-root")
    message(FATAL_ERROR "CLI entry input must remain relative to the explicit model root")
endif()
