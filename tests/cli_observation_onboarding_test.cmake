foreach(required
        LTFA_CLI
        LTFA_EXAMPLE_ROOT
        LTFA_README
        LTFA_DEVELOPMENT
        LTFA_OBSERVATION_MANUAL
        LTFA_COMPLETION_SCRIPT
        LTFA_TEST_ROOT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "missing required setting ${required}")
    endif()
endforeach()

function(require_text content expected label)
    string(FIND "${content}" "${expected}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "${label} is missing ${expected}")
    endif()
endfunction()

function(check_observation_route label expected_title)
    execute_process(
        COMMAND "${LTFA_CLI}" guide ${ARGN}
        RESULT_VARIABLE text_result
        OUTPUT_VARIABLE text_output
        ERROR_VARIABLE text_error
    )
    execute_process(
        COMMAND "${LTFA_CLI}" guide ${ARGN} --format json
        RESULT_VARIABLE json_result
        OUTPUT_VARIABLE json_output
        ERROR_VARIABLE json_error
    )
    string(JSON domain GET "${json_output}" domain)
    string(JSON schema GET "${json_output}" observation_contract schema)
    if(NOT text_result EQUAL 0 OR NOT text_error STREQUAL "" OR
       NOT text_output MATCHES "^${expected_title}" OR
       NOT text_output MATCHES "Topic: observations" OR
       NOT json_result EQUAL 0 OR NOT json_error STREQUAL "" OR
       NOT domain STREQUAL "observation-data" OR NOT schema STREQUAL "ltfa.observation/v1")
        message(FATAL_ERROR "observation Guide route ${label} is incorrect")
    endif()
endfunction()

check_observation_route(root "Observation JSON Lines" observations)
check_observation_route(format "Observation framing and bounds" observations format)
check_observation_route(fields "Observation fields" observations fields)
check_observation_route(semantics "Observation semantics" observations semantics)
check_observation_route(errors "Observation diagnostics" observations errors)

execute_process(
    COMMAND "${LTFA_CLI}" guide samples diagnosis
    RESULT_VARIABLE sample_text_result
    OUTPUT_VARIABLE sample_text_output
    ERROR_VARIABLE sample_text_error
)
execute_process(
    COMMAND "${LTFA_CLI}" guide samples diagnosis --format json
    RESULT_VARIABLE sample_json_result
    OUTPUT_VARIABLE sample_json_output
    ERROR_VARIABLE sample_json_error
)
string(JSON sample_domain GET "${sample_json_output}" domain)
if(NOT sample_text_result EQUAL 0 OR NOT sample_text_error STREQUAL "" OR
   NOT sample_text_output MATCHES "^Sample: diagnosis" OR
   NOT sample_json_result EQUAL 0 OR NOT sample_json_error STREQUAL "" OR
   NOT sample_domain STREQUAL "observation-data")
    message(FATAL_ERROR "diagnosis sample Guide route is incorrect")
endif()

execute_process(
    COMMAND "${LTFA_CLI}" guide observations fields --format json
    RESULT_VARIABLE guide_result
    OUTPUT_VARIABLE guide_output
    ERROR_VARIABLE guide_error
)
if(NOT guide_result EQUAL 0 OR NOT guide_error STREQUAL "")
    message(FATAL_ERROR "observation Guide failed: ${guide_error}")
endif()

string(JSON guide_schema GET "${guide_output}" schema)
string(JSON guide_contract_version GET "${guide_output}" cli_contract_version)
string(JSON guide_domain GET "${guide_output}" domain)
string(JSON observation_schema GET "${guide_output}" observation_contract schema)
string(JSON observation_framing GET "${guide_output}" observation_contract framing)
string(JSON observation_manual GET "${guide_output}" navigation observation_reference)
string(JSON diagnosis_sample GET "${guide_output}" navigation diagnosis_sample)
if(NOT guide_schema STREQUAL "ltfa.language-guide/v1" OR
   NOT guide_contract_version EQUAL 5 OR NOT guide_domain STREQUAL "observation-data" OR
   NOT observation_schema STREQUAL "ltfa.observation/v1" OR
   NOT observation_framing STREQUAL "json-lines" OR
   NOT observation_manual STREQUAL
       "share/doc/ltfa/docs/observations/ltfa-observation-v1.md" OR
   NOT diagnosis_sample STREQUAL "share/doc/ltfa/examples/diagnosis")
    message(FATAL_ERROR "structured observation Guide contract is incorrect")
endif()

set(required_fields schema probe_id target vantage kind outcome observed_at valid_for_ms)
set(optional_fields duration_ms value unit state producer_sequence producer_id evidence_ref detail)
set(kinds icmp tcp-connect custom-heartbeat metric discrete-state)
set(outcomes pass fail timeout error unknown)

foreach(inventory IN ITEMS required_fields optional_fields kinds outcomes)
    list(LENGTH ${inventory} expected_count)
    string(JSON actual_count LENGTH "${guide_output}" observation_contract ${inventory})
    if(NOT actual_count EQUAL expected_count)
        message(FATAL_ERROR "${inventory} count differs from the accepted contract")
    endif()
    math(EXPR last_index "${expected_count} - 1")
    foreach(index RANGE 0 ${last_index})
        list(GET ${inventory} ${index} expected)
        string(JSON actual GET "${guide_output}" observation_contract ${inventory} ${index})
        if(NOT actual STREQUAL expected)
            message(FATAL_ERROR
                "${inventory}[${index}] expected ${expected}, received ${actual}"
            )
        endif()
    endforeach()
endforeach()

execute_process(
    COMMAND "${LTFA_CLI}" help diagnose --format json
    RESULT_VARIABLE help_result
    OUTPUT_VARIABLE help_output
    ERROR_VARIABLE help_error
)
string(JSON help_schema GET "${help_output}" schema)
string(JSON help_contract_version GET "${help_output}" cli_contract_version)
string(JSON help_guide GET "${help_output}" commands 0 navigation observation_guide)
string(JSON help_manual GET "${help_output}" commands 0 navigation observation_manual)
string(JSON help_sample GET "${help_output}" commands 0 navigation diagnosis_sample)
if(NOT help_result EQUAL 0 OR NOT help_error STREQUAL "" OR
   NOT help_schema STREQUAL "ltfa.command-help/v1" OR NOT help_contract_version EQUAL 5 OR
   NOT help_guide STREQUAL "ltfa guide observations" OR
   NOT help_manual STREQUAL "share/doc/ltfa/docs/observations/ltfa-observation-v1.md" OR
   NOT help_sample STREQUAL "share/doc/ltfa/examples/diagnosis")
    message(FATAL_ERROR "diagnose help is not synchronized with the observation Guide")
endif()

file(READ "${LTFA_README}" readme)
file(READ "${LTFA_DEVELOPMENT}" development)
file(READ "${LTFA_OBSERVATION_MANUAL}" manual)
file(READ "${LTFA_COMPLETION_SCRIPT}" completion_script)

foreach(value IN ITEMS
        "ltfa.observation/v1"
        "ltfa guide observations"
        "ltfa guide samples diagnosis"
        "examples/diagnosis")
    require_text("${readme}" "${value}" "README")
    require_text("${development}" "${value}" "development guide")
    require_text("${manual}" "${value}" "observation manual")
endforeach()

foreach(inventory IN ITEMS required_fields optional_fields kinds outcomes)
    foreach(value IN LISTS ${inventory})
        require_text("${manual}" "${value}" "observation manual")
    endforeach()
endforeach()

string(FIND "${completion_script}" "observations" copied_observation_topic)
string(FIND "${completion_script}" "diagnosis" copied_diagnosis_sample)
if(NOT copied_observation_topic EQUAL -1 OR NOT copied_diagnosis_sample EQUAL -1)
    message(FATAL_ERROR "bash completion copied Guide candidates instead of querying descriptors")
endif()

execute_process(
    COMMAND "${LTFA_CLI}" __completion --bash guide observations f
    RESULT_VARIABLE completion_result
    OUTPUT_VARIABLE completion_output
    ERROR_VARIABLE completion_error
)
if(NOT completion_result EQUAL 0 OR NOT completion_error STREQUAL "" OR
   NOT completion_output STREQUAL "__ltfa_completion_mode=plain\nformat\nfields\n")
    message(FATAL_ERROR "observation completion is not descriptor-backed: ${completion_output}")
endif()

execute_process(
    COMMAND "${LTFA_CLI}" guide observatons --format json
    RESULT_VARIABLE unknown_topic_result
    OUTPUT_VARIABLE unknown_topic_output
    ERROR_VARIABLE unknown_topic_error
)
string(JSON unknown_topic_suggestion GET "${unknown_topic_output}" usage suggestion)
if(NOT unknown_topic_result EQUAL 2 OR NOT unknown_topic_error STREQUAL "" OR
   NOT unknown_topic_suggestion STREQUAL "observations")
    message(FATAL_ERROR "observation topic recovery is incorrect")
endif()

execute_process(
    COMMAND "${LTFA_CLI}" guide observations fieldz --format json
    RESULT_VARIABLE unknown_subtopic_result
    OUTPUT_VARIABLE unknown_subtopic_output
    ERROR_VARIABLE unknown_subtopic_error
)
string(JSON unknown_subtopic_suggestion GET "${unknown_subtopic_output}" usage suggestion)
string(JSON unknown_subtopic_guide GET "${unknown_subtopic_output}" actions 1 invocation)
if(NOT unknown_subtopic_result EQUAL 2 OR NOT unknown_subtopic_error STREQUAL "" OR
   NOT unknown_subtopic_suggestion STREQUAL "fields" OR
   NOT unknown_subtopic_guide STREQUAL "ltfa guide observations")
    message(FATAL_ERROR "observation subtopic recovery is incorrect")
endif()

file(REMOVE_RECURSE "${LTFA_TEST_ROOT}")
file(MAKE_DIRECTORY "${LTFA_TEST_ROOT}")
file(COPY "${LTFA_EXAMPLE_ROOT}/diagnosis/" DESTINATION "${LTFA_TEST_ROOT}")
file(WRITE "${LTFA_TEST_ROOT}/invalid.jsonl" "{\"schema\":\"ltfa.observation/v1\"\n")
file(WRITE "${LTFA_TEST_ROOT}/future.jsonl"
    "{\"schema\":\"ltfa.observation/v2\",\"probe_id\":\"probe:worker\","
    "\"target\":\"process:worker\",\"vantage\":\"process:observer\",\"kind\":\"icmp\","
    "\"outcome\":\"pass\",\"observed_at\":\"2030-01-01T00:10:00Z\","
    "\"valid_for_ms\":60000}\n"
)
file(WRITE "${LTFA_TEST_ROOT}/invalid-time.jsonl"
    "{\"schema\":\"ltfa.observation/v1\",\"probe_id\":\"probe:worker\","
    "\"target\":\"process:worker\",\"vantage\":\"process:observer\",\"kind\":\"icmp\","
    "\"outcome\":\"pass\",\"observed_at\":\"not-a-time\",\"valid_for_ms\":60000}\n"
)
file(WRITE "${LTFA_TEST_ROOT}/mismatch.jsonl"
    "{\"schema\":\"ltfa.observation/v1\",\"probe_id\":\"probe:unknown\","
    "\"target\":\"process:worker\",\"vantage\":\"process:observer\",\"kind\":\"icmp\","
    "\"outcome\":\"pass\",\"observed_at\":\"2030-01-01T00:10:00Z\","
    "\"valid_for_ms\":60000}\n"
)
file(WRITE "${LTFA_TEST_ROOT}/restricted.ltfa"
    "ltfa 1\n"
    "scope site:restricted kind=site completeness=partial evidence-quality=inferred\n"
    "entity restricted:target kind=device plane=communication layer=physical "
        "scope=site:restricted sensitivity=restricted\n"
    "entity public:observer kind=process plane=service layer=application "
        "scope=site:restricted\n"
    "failure failure:restricted target=restricted:target mode=device-failure\n"
    "probe probe:restricted target=restricted:target vantage=public:observer kind=icmp "
        "valid-for=60s\n"
)
file(WRITE "${LTFA_TEST_ROOT}/restricted-observations.jsonl"
    "{\"schema\":\"ltfa.observation/v1\",\"probe_id\":\"probe:restricted\","
    "\"target\":\"restricted:target\",\"vantage\":\"public:observer\","
    "\"kind\":\"icmp\",\"outcome\":\"timeout\","
    "\"observed_at\":\"2030-01-01T00:10:00Z\",\"valid_for_ms\":60000,"
    "\"producer_id\":\"must-not-render\",\"detail\":\"must-not-render\"}\n"
)

function(check_observation_error label observations analysis_at expected_code)
    execute_process(
        COMMAND "${LTFA_CLI}" diagnose --model-root "${LTFA_TEST_ROOT}" --input main.ltfa
            --observations "${observations}" --analysis-at "${analysis_at}" --format json
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(result EQUAL 0 OR NOT error STREQUAL "")
        message(FATAL_ERROR "${label} did not produce a structured failure")
    endif()
    string(JSON code GET "${output}" diagnostics 0 code)
    string(JSON action_count LENGTH "${output}" actions)
    string(JSON guide_action GET "${output}" actions 1 invocation)
    string(JSON help_action GET "${output}" actions 2 invocation)
    if(NOT code STREQUAL "${expected_code}" OR NOT action_count EQUAL 3 OR
       NOT guide_action STREQUAL "ltfa guide observations errors" OR
       NOT help_action STREQUAL "ltfa help diagnose")
        message(FATAL_ERROR "${label} navigation is incorrect: ${output}")
    endif()
endfunction()

check_observation_error(missing missing.jsonl 2030-01-01T00:10:30Z LTOBS-001)
check_observation_error(reference-time observations.jsonl not-a-time LTOBS-002)
check_observation_error(malformed invalid.jsonl 2030-01-01T00:10:30Z LTOBS-003)
check_observation_error(future-schema future.jsonl 2030-01-01T00:10:30Z LTOBS-003)
check_observation_error(observed-at invalid-time.jsonl 2030-01-01T00:10:30Z LTOBS-003)
check_observation_error(contract-mismatch mismatch.jsonl 2030-01-01T00:10:30Z LTOBS-004)

execute_process(
    COMMAND "${LTFA_CLI}" diagnose --model-root "${LTFA_TEST_ROOT}" --input main.ltfa
        --observations missing.jsonl --analysis-at 2030-01-01T00:10:30Z
    RESULT_VARIABLE text_error_result
    OUTPUT_VARIABLE text_error_output
    ERROR_VARIABLE text_error
)
if(text_error_result EQUAL 0 OR NOT text_error_output STREQUAL "" OR
   NOT text_error MATCHES "guide: ltfa guide observations errors" OR
   NOT text_error MATCHES "help: ltfa help diagnose")
    message(FATAL_ERROR "text observation navigation is incorrect: ${text_error}")
endif()

set(restricted_command
    diagnose --model-root "${LTFA_TEST_ROOT}" --input restricted.ltfa
    --observations restricted-observations.jsonl --analysis-at 2030-01-01T00:10:30Z
    --format json
)
execute_process(
    COMMAND "${LTFA_CLI}" ${restricted_command}
    RESULT_VARIABLE restricted_default_result
    OUTPUT_VARIABLE restricted_default_output
    ERROR_VARIABLE restricted_default_error
)
execute_process(
    COMMAND "${LTFA_CLI}" ${restricted_command} --include-restricted
    RESULT_VARIABLE restricted_explicit_result
    OUTPUT_VARIABLE restricted_explicit_output
    ERROR_VARIABLE restricted_explicit_error
)
if(restricted_default_result EQUAL 0 OR NOT restricted_default_error STREQUAL "" OR
   restricted_default_output MATCHES "restricted:target|probe:restricted" OR
   NOT restricted_explicit_result EQUAL 0 OR NOT restricted_explicit_error STREQUAL "" OR
   NOT restricted_explicit_output MATCHES "probe:restricted" OR
   restricted_explicit_output MATCHES "must-not-render")
    message(FATAL_ERROR "restricted diagnosis output policy is incorrect")
endif()
