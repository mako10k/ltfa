foreach(required LTFA_CLI LTFA_SLICE2_ROOT LTFA_HISTORY_ROOT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "missing required setting ${required}")
    endif()
endforeach()

foreach(command cuts diagnose)
    execute_process(
        COMMAND "${LTFA_CLI}" help "${command}" --format json
        RESULT_VARIABLE help_result OUTPUT_VARIABLE help_output ERROR_VARIABLE help_error
    )
    string(JSON help_operation ERROR_VARIABLE help_json_error GET "${help_output}" commands 0 operation)
    if(NOT help_result EQUAL 0 OR NOT help_error STREQUAL "" OR help_json_error OR
       NOT help_operation STREQUAL "${command}")
        message(FATAL_ERROR "${command} structured help contract failed")
    endif()
endforeach()

execute_process(
    COMMAND "${LTFA_CLI}" cuts --model-root "${LTFA_SLICE2_ROOT}" --input main.ltfa
        --scope site:verified --source device:path-source --target device:path-target --format json
    RESULT_VARIABLE cuts_result OUTPUT_VARIABLE cuts_output ERROR_VARIABLE cuts_error
)
if(NOT cuts_result EQUAL 0 OR NOT cuts_error STREQUAL "")
    message(FATAL_ERROR "cuts command failed: ${cuts_error}")
endif()
string(JSON cuts_schema GET "${cuts_output}" schema)
string(JSON object_cut GET "${cuts_output}" minimum_object_cut)
string(JSON relation_cut GET "${cuts_output}" minimum_relation_cut)
if(NOT cuts_schema STREQUAL "ltfa.service-cuts/v1" OR
   NOT object_cut EQUAL 2 OR NOT relation_cut EQUAL 2)
    message(FATAL_ERROR "cuts JSON contract mismatch")
endif()

execute_process(
    COMMAND "${LTFA_CLI}" cuts --model-root "${LTFA_SLICE2_ROOT}" --input main.ltfa
        --scope site:verified --source device:path-source --target device:path-target
        --max-candidate-sets 1 --format json
    RESULT_VARIABLE bounded_cuts_result OUTPUT_VARIABLE bounded_cuts_output
    ERROR_VARIABLE bounded_cuts_error
)
string(JSON bounded_cuts_status GET "${bounded_cuts_output}" status)
if(NOT bounded_cuts_result EQUAL 0 OR NOT bounded_cuts_error STREQUAL "" OR
   NOT bounded_cuts_status STREQUAL "incomplete")
    message(FATAL_ERROR "bounded cuts must return exit 0 structured incomplete")
endif()

execute_process(
    COMMAND "${LTFA_CLI}" diagnose --model-root "${LTFA_HISTORY_ROOT}" --input main.ltfa
        --observations observations.jsonl --analysis-at 2030-01-01T00:10:30Z --format json
    RESULT_VARIABLE diagnosis_result OUTPUT_VARIABLE diagnosis_output ERROR_VARIABLE diagnosis_error
)
if(NOT diagnosis_result EQUAL 0 OR NOT diagnosis_error STREQUAL "")
    message(FATAL_ERROR "diagnose command failed: ${diagnosis_error}")
endif()
string(JSON diagnosis_schema GET "${diagnosis_output}" schema)
string(JSON diagnosis_status GET "${diagnosis_output}" diagnosis status)
string(JSON recommendation_status GET "${diagnosis_output}" next_observations status)
if(NOT diagnosis_schema STREQUAL "ltfa.diagnosis-cli/v1" OR
   NOT diagnosis_status STREQUAL "complete" OR
   NOT recommendation_status STREQUAL "complete")
    message(FATAL_ERROR "diagnose JSON contract mismatch")
endif()

execute_process(
    COMMAND "${LTFA_CLI}" diagnose --model-root "${LTFA_HISTORY_ROOT}" --input main.ltfa
        --observations observations.jsonl --analysis-at 2030-01-01T00:10:30Z
        --max-hypotheses 1 --format json
    RESULT_VARIABLE bounded_diagnosis_result OUTPUT_VARIABLE bounded_diagnosis_output
    ERROR_VARIABLE bounded_diagnosis_error
)
string(JSON bounded_diagnosis_status GET "${bounded_diagnosis_output}" diagnosis status)
string(JSON bounded_recommendation_status GET "${bounded_diagnosis_output}" next_observations status)
if(NOT bounded_diagnosis_result EQUAL 0 OR NOT bounded_diagnosis_error STREQUAL "" OR
   NOT bounded_diagnosis_status STREQUAL "incomplete" OR
   NOT bounded_recommendation_status STREQUAL "incomplete")
    message(FATAL_ERROR "bounded diagnosis must return exit 0 structured incomplete")
endif()

execute_process(
    COMMAND "${LTFA_CLI}" diagnose --model-root "${LTFA_HISTORY_ROOT}" --input main.ltfa
        --observations observations.jsonl --analysis-at 2030-01-01T00:10:30Z --format dot
    RESULT_VARIABLE dot_result OUTPUT_VARIABLE dot_output ERROR_VARIABLE dot_error
)
if(NOT dot_result EQUAL 0 OR NOT dot_error STREQUAL "" OR
   NOT dot_output MATCHES "^digraph ltfa_diagnosis")
    message(FATAL_ERROR "diagnosis DOT CLI contract mismatch")
endif()

execute_process(
    COMMAND "${LTFA_CLI}" diagnose --model-root "${LTFA_HISTORY_ROOT}" --input main.ltfa
        --observations ../slice2/main.ltfa
        --analysis-at 2030-01-01T00:10:30Z --format json
    RESULT_VARIABLE escape_result OUTPUT_VARIABLE escape_output ERROR_VARIABLE escape_error
)
if(NOT escape_result EQUAL 3 OR NOT escape_output MATCHES "LTOBS-001")
    message(FATAL_ERROR "observation traversal must fail as structured input error")
endif()

execute_process(
    COMMAND "${LTFA_CLI}" cuts --model-root "${LTFA_SLICE2_ROOT}" --input main.ltfa
        --scope site:verified --source device:path-source --target device:path-target
        --max-cut-size 3 --format json
    RESULT_VARIABLE usage_result OUTPUT_VARIABLE usage_output ERROR_VARIABLE usage_error
)
if(NOT usage_result EQUAL 2 OR NOT usage_output MATCHES "invalid_option_value")
    message(FATAL_ERROR "invalid cut size must be a structured usage error")
endif()
