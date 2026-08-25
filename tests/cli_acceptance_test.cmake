foreach(required
        LTFA_CLI
        LTFA_EXPECTED_VERSION
        LTFA_VALID_ROOT
        LTFA_INVALID_ROOT
        LTFA_SLICE2_ROOT)
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

run_cli(no_arguments)
if(NOT no_arguments_result EQUAL 2 OR NOT no_arguments_output STREQUAL "" OR
   NOT no_arguments_error MATCHES
       "^LTCLI-001 error: expected a command\n"
       OR NOT no_arguments_error MATCHES
       "  next: Choose a command, then rerun ltfa[.]\n  help: ltfa help\n$")
    message(FATAL_ERROR "no-argument invocation must be a stable usage failure")
endif()

run_cli(root_help --help)
run_cli(root_help_short -h)
run_cli(root_help_command help)
if(NOT root_help_result EQUAL 0 OR NOT root_help_short_result EQUAL 0 OR
   NOT root_help_command_result EQUAL 0)
    message(FATAL_ERROR "root help routes must succeed")
endif()
if(NOT root_help_error STREQUAL "" OR NOT root_help_short_error STREQUAL "" OR
   NOT root_help_command_error STREQUAL "")
    message(FATAL_ERROR "successful root help must not write stderr")
endif()
if(NOT root_help_output STREQUAL root_help_short_output OR
   NOT root_help_output STREQUAL root_help_command_output OR
   NOT root_help_output MATCHES "The current core is offline" OR
   NOT root_help_output MATCHES "Bash completion:" OR
   NOT root_help_output MATCHES "Guides:" OR
   NOT root_help_output MATCHES "ltfa guide syntax" OR
   NOT root_help_output MATCHES "ltfa guide observations" OR
   NOT root_help_output MATCHES "ltfa help <command> --format json")
    message(FATAL_ERROR "root help routes must be equivalent and state the offline boundary")
endif()

run_cli(model_help model --help)
run_cli(model_help_short model -h)
run_cli(model_help_command help model)
run_cli(analyze_help analyze --help)
run_cli(analyze_help_short analyze -h)
run_cli(analyze_help_command help analyze)
if(NOT model_help_result EQUAL 0 OR NOT model_help_short_result EQUAL 0 OR
   NOT model_help_command_result EQUAL 0 OR NOT analyze_help_result EQUAL 0 OR
   NOT analyze_help_short_result EQUAL 0 OR NOT analyze_help_command_result EQUAL 0)
    message(FATAL_ERROR "command help routes must succeed")
endif()
if(NOT model_help_error STREQUAL "" OR NOT model_help_short_error STREQUAL "" OR
   NOT model_help_command_error STREQUAL "" OR NOT analyze_help_error STREQUAL "" OR
   NOT analyze_help_short_error STREQUAL "" OR NOT analyze_help_command_error STREQUAL "")
    message(FATAL_ERROR "successful command help must not write stderr")
endif()
if(NOT model_help_output STREQUAL model_help_short_output OR
   NOT model_help_output STREQUAL model_help_command_output OR
   NOT model_help_output MATCHES "^Command: ltfa model" OR
   NOT model_help_output MATCHES "Exit statuses:" OR
   NOT model_help_output MATCHES
       "Navigation:\n  help: ltfa help model\n  json-help: ltfa help model --format json")
    message(FATAL_ERROR "model help routes must expose one detailed contract")
endif()
if(NOT analyze_help_output STREQUAL analyze_help_short_output OR
   NOT analyze_help_output STREQUAL analyze_help_command_output OR
   NOT analyze_help_output MATCHES "^Command: ltfa analyze" OR
   NOT analyze_help_output MATCHES "Effect: read" OR
   NOT analyze_help_output MATCHES "ltfa.assurance/v1")
    message(FATAL_ERROR "analyze help routes must expose one detailed offline contract")
endif()

run_cli(help_json_first help --format json)
run_cli(help_json_second help --format=json)
if(NOT help_json_first_result EQUAL 0 OR NOT help_json_second_result EQUAL 0 OR
   NOT help_json_first_error STREQUAL "" OR NOT help_json_second_error STREQUAL "")
    message(FATAL_ERROR "structured root help must succeed without stderr")
endif()
if(NOT help_json_first_output STREQUAL help_json_second_output)
    message(FATAL_ERROR "structured root help must be byte-stable across option spellings")
endif()
string(JSON help_schema GET "${help_json_first_output}" schema)
string(JSON help_contract GET "${help_json_first_output}" cli_contract_version)
string(JSON help_tool_version GET "${help_json_first_output}" tool_version)
string(JSON help_operation GET "${help_json_first_output}" operation)
string(JSON help_command_count LENGTH "${help_json_first_output}" commands)
if(NOT help_schema STREQUAL "ltfa.command-help/v1" OR NOT help_contract EQUAL 5 OR
   NOT help_tool_version STREQUAL "${LTFA_EXPECTED_VERSION}" OR
   NOT help_operation STREQUAL "help" OR NOT help_command_count EQUAL 6)
    message(FATAL_ERROR "structured root help contract is incorrect")
endif()

run_cli(analyze_help_json_first help analyze --format json)
run_cli(analyze_help_json_second help analyze --format=json)
if(NOT analyze_help_json_first_result EQUAL 0 OR
   NOT analyze_help_json_second_result EQUAL 0 OR
   NOT analyze_help_json_first_error STREQUAL "" OR
   NOT analyze_help_json_second_error STREQUAL "" OR
   NOT analyze_help_json_first_output STREQUAL analyze_help_json_second_output)
    message(FATAL_ERROR "structured analyze help must be byte-stable")
endif()
string(JSON analyze_help_query GET "${analyze_help_json_first_output}" query command)
string(JSON analyze_help_count LENGTH "${analyze_help_json_first_output}" commands)
string(JSON analyze_help_operation
    GET "${analyze_help_json_first_output}" commands 0 operation
)
string(JSON analyze_help_effect GET "${analyze_help_json_first_output}" commands 0 effect)
string(JSON analyze_help_payload
    GET "${analyze_help_json_first_output}" commands 0 output payload
)
string(JSON analyze_help_option_count
    LENGTH "${analyze_help_json_first_output}" commands 0 options
)
string(JSON analyze_help_schema_count
    LENGTH "${analyze_help_json_first_output}" commands 0 result_schemas
)
string(JSON analyze_help_exit_count
    LENGTH "${analyze_help_json_first_output}" commands 0 exit_statuses
)
string(JSON analyze_help_usage GET "${analyze_help_json_first_output}" commands 0 usage)
string(JSON analyze_help_navigation GET
    "${analyze_help_json_first_output}" commands 0 navigation help
)
string(JSON root_help_navigation GET
    "${analyze_help_json_first_output}" navigation root_help
)
string(JSON bash_completion_location GET
    "${analyze_help_json_first_output}" navigation bash_completion installed_location
)
string(JSON language_guide_navigation GET
    "${analyze_help_json_first_output}" commands 0 navigation language_guide
)
string(CONCAT expected_analyze_usage
    "ltfa analyze --model-root <directory> --input <file> --scope <scope-id> "
    "--source <object-id> --target <object-id> [--failure <failure-id>] "
    "[--format human|json] [--include-restricted] [--max-candidates <count>] "
    "[--max-visited-objects <count>] [--max-examined-relations <count>] "
    "[--max-events <count>] [--max-state-transitions <count>]"
)
if(NOT analyze_help_query STREQUAL "analyze" OR NOT analyze_help_count EQUAL 1 OR
   NOT analyze_help_operation STREQUAL "analyze" OR
   NOT analyze_help_effect STREQUAL "read" OR
   NOT analyze_help_payload STREQUAL "assurance" OR
   NOT analyze_help_option_count EQUAL 14 OR
   NOT analyze_help_schema_count EQUAL 2 OR NOT analyze_help_exit_count EQUAL 5 OR
   NOT analyze_help_usage STREQUAL expected_analyze_usage OR
   NOT analyze_help_navigation STREQUAL "ltfa help analyze" OR
   NOT root_help_navigation STREQUAL "ltfa help" OR
   NOT language_guide_navigation STREQUAL "ltfa guide workflows analyze" OR
   NOT bash_completion_location STREQUAL "share/bash-completion/completions/ltfa")
    message(FATAL_ERROR "structured analyze help is incomplete")
endif()

set(analyze_budget_names
    max-candidates
    max-visited-objects
    max-examined-relations
    max-events
    max-state-transitions
)
set(analyze_budget_defaults
    1024
    2000000
    10000000
    1000000
    300000
)
foreach(index RANGE 0 4)
    math(EXPR option_index "${index} + 9")
    list(GET analyze_budget_names ${index} expected_name)
    list(GET analyze_budget_defaults ${index} expected_default)
    string(JSON actual_name
        GET "${analyze_help_json_first_output}" commands 0 options ${option_index} name
    )
    string(JSON actual_type
        GET "${analyze_help_json_first_output}" commands 0 options ${option_index} value_type
    )
    string(JSON actual_default
        GET "${analyze_help_json_first_output}" commands 0 options ${option_index} default
    )
    if(NOT actual_name STREQUAL expected_name OR
       NOT actual_type STREQUAL "positive-integer" OR
       NOT actual_default STREQUAL expected_default)
        message(FATAL_ERROR "structured analyze budget option ${expected_name} is incorrect")
    endif()
endforeach()

set(analyze_command
    analyze
    --model-root "${LTFA_SLICE2_ROOT}"
    --input main.ltfa
    --scope site:verified
    --source device:path-source
    --target device:path-tail
    --failure failure:breaker
)

run_cli(analyze_json_first ${analyze_command} --format json)
run_cli(analyze_json_second ${analyze_command} --format=json)
if(NOT analyze_json_first_result EQUAL 0 OR NOT analyze_json_second_result EQUAL 0 OR
   NOT analyze_json_first_error STREQUAL "" OR NOT analyze_json_second_error STREQUAL "" OR
   NOT analyze_json_first_output STREQUAL analyze_json_second_output)
    message(FATAL_ERROR "analyze JSON must succeed and remain byte-stable")
endif()
string(JSON analyze_schema GET "${analyze_json_first_output}" schema)
string(JSON analyze_operation GET "${analyze_json_first_output}" operation)
string(JSON analyze_result GET "${analyze_json_first_output}" result)
string(JSON analyze_reachable GET "${analyze_json_first_output}" path reachable)
string(JSON analyze_spof_count LENGTH "${analyze_json_first_output}" spof findings)
string(JSON analyze_event_count LENGTH "${analyze_json_first_output}" propagation events)
if(NOT analyze_schema STREQUAL "ltfa.assurance/v1" OR
   NOT analyze_operation STREQUAL "analyze" OR NOT analyze_result STREQUAL "complete" OR
   NOT analyze_reachable OR NOT analyze_spof_count EQUAL 3 OR
   NOT analyze_event_count EQUAL 4)
    message(FATAL_ERROR "analyze JSON result does not satisfy the Slice 2 contract")
endif()

run_cli(analyze_human ${analyze_command} --format human)
if(NOT analyze_human_result EQUAL 0 OR NOT analyze_human_error STREQUAL "" OR
   NOT analyze_human_output MATCHES
       "^result: complete\nschema: ltfa.assurance/v1\noperation: analyze" OR
   NOT analyze_human_output MATCHES "classification: confirmed-spof" OR
   NOT analyze_human_output MATCHES "at-ms: 30000\n      object: service:held")
    message(FATAL_ERROR "human analyze output must lead with and retain the result")
endif()

run_cli(analyze_partial
    analyze --model-root "${LTFA_SLICE2_ROOT}" --input main.ltfa
    --scope site:partial --source device:partial-source --target device:partial-tail
    --format json
)
string(JSON partial_count LENGTH "${analyze_partial_output}" spof findings)
if(NOT analyze_partial_result EQUAL 0 OR NOT analyze_partial_error STREQUAL "" OR
   NOT partial_count EQUAL 3)
    message(FATAL_ERROR "partial analyze fixture must return its modeled findings")
endif()
math(EXPR partial_last "${partial_count} - 1")
foreach(index RANGE 0 ${partial_last})
    string(JSON partial_classification
        GET "${analyze_partial_output}" spof findings ${index} classification
    )
    if(NOT partial_classification STREQUAL "model-spof")
        message(FATAL_ERROR "partial analysis emitted an unqualified SPOF")
    endif()
endforeach()

run_cli(analyze_unreachable
    analyze --model-root "${LTFA_SLICE2_ROOT}" --input main.ltfa
    --scope site:verified --source device:path-source --target facility:breaker
    --format json
)
string(JSON unreachable_result GET "${analyze_unreachable_output}" result)
string(JSON unreachable_path GET "${analyze_unreachable_output}" path reachable)
string(JSON unreachable_spof_status GET "${analyze_unreachable_output}" spof status)
if(NOT analyze_unreachable_result EQUAL 0 OR NOT unreachable_result STREQUAL "complete" OR
   unreachable_path OR NOT unreachable_spof_status STREQUAL "not-applicable")
    message(FATAL_ERROR "unreachable must be a successful non-SPOF finding")
endif()

function(check_bounded_budget option)
    run_cli(bounded_budget ${analyze_command} "--${option}" 1 --format json)
    if(NOT bounded_budget_result EQUAL 0 OR NOT bounded_budget_error STREQUAL "")
        message(FATAL_ERROR "valid --${option} budget must remain a successful result")
    endif()
    string(JSON budget_result GET "${bounded_budget_output}" result)
    if(NOT budget_result STREQUAL "incomplete")
        message(FATAL_ERROR "--${option}=1 must report bounded incompleteness")
    endif()
endfunction()

foreach(option IN LISTS analyze_budget_names)
    check_bounded_budget("${option}")
endforeach()

function(check_invalid_budget option value)
    run_cli(invalid_budget ${analyze_command} "--${option}" "${value}" --format json)
    if(NOT invalid_budget_result EQUAL 2 OR NOT invalid_budget_error STREQUAL "")
        message(FATAL_ERROR "invalid --${option}=${value} must be a JSON usage error")
    endif()
    string(JSON invalid_schema GET "${invalid_budget_output}" schema)
    string(JSON invalid_status GET "${invalid_budget_output}" exit_status)
    string(JSON invalid_kind GET "${invalid_budget_output}" usage kind)
    string(JSON invalid_token GET "${invalid_budget_output}" usage token)
    string(JSON invalid_message GET "${invalid_budget_output}" diagnostics 0 message)
    if(NOT invalid_schema STREQUAL "ltfa.cli-error/v1" OR NOT invalid_status EQUAL 2 OR
       NOT invalid_kind STREQUAL "invalid_option_value" OR
       NOT invalid_token STREQUAL value OR
       NOT invalid_message STREQUAL
           "option --${option} must be a positive base-10 integer")
        message(FATAL_ERROR "invalid --${option}=${value} diagnostic is incorrect")
    endif()
endfunction()

foreach(option IN LISTS analyze_budget_names)
    check_invalid_budget("${option}" 0)
endforeach()
check_invalid_budget("max-candidates" "not-a-count")
check_invalid_budget("max-events" "18446744073709551616")

run_cli(analyze_unknown_scope
    analyze --model-root "${LTFA_SLICE2_ROOT}" --input main.ltfa
    --scope site:missing --source device:path-source --target device:path-tail
    --format json
)
if(NOT analyze_unknown_scope_result EQUAL 4 OR NOT analyze_unknown_scope_error STREQUAL "")
    message(FATAL_ERROR "unknown analysis selection must use exit 4 on JSON stdout")
endif()
string(JSON selection_schema GET "${analyze_unknown_scope_output}" schema)
string(JSON selection_code GET "${analyze_unknown_scope_output}" diagnostics 0 code)
string(JSON selection_next GET "${analyze_unknown_scope_output}" actions 0 description)
string(JSON selection_help GET "${analyze_unknown_scope_output}" actions 1 invocation)
if(NOT selection_schema STREQUAL "ltfa.cli-error/v1" OR
   NOT selection_code STREQUAL "LTANL-001" OR
   NOT selection_next STREQUAL
       "Correct the reported selection, then rerun ltfa analyze." OR
   NOT selection_help STREQUAL "ltfa help analyze")
    message(FATAL_ERROR "analysis selection diagnostic contract is incorrect")
endif()

function(check_selection_rejection expected_message)
    run_cli(selection_rejection
        analyze --model-root "${LTFA_SLICE2_ROOT}" --input main.ltfa
        --scope site:verified ${ARGN} --format json
    )
    if(NOT selection_rejection_result EQUAL 4 OR
       NOT selection_rejection_error STREQUAL "")
        message(FATAL_ERROR "analysis selection rejection must use exit 4 on JSON stdout")
    endif()
    string(JSON rejection_schema GET "${selection_rejection_output}" schema)
    string(JSON rejection_operation GET "${selection_rejection_output}" operation)
    string(JSON rejection_status GET "${selection_rejection_output}" exit_status)
    string(JSON rejection_code GET "${selection_rejection_output}" diagnostics 0 code)
    string(JSON rejection_message GET "${selection_rejection_output}" diagnostics 0 message)
    if(NOT rejection_schema STREQUAL "ltfa.cli-error/v1" OR
       NOT rejection_operation STREQUAL "analyze" OR NOT rejection_status EQUAL 4 OR
       NOT rejection_code STREQUAL "LTANL-001" OR
       NOT rejection_message STREQUAL expected_message)
        message(FATAL_ERROR "analysis selection rejection is incorrect: ${expected_message}")
    endif()
endfunction()

check_selection_rejection(
    "source object not found: device:missing"
    --source device:missing --target device:path-tail
)
check_selection_rejection(
    "target object not found: device:missing"
    --source device:path-source --target device:missing
)
check_selection_rejection(
    "source and target must be distinct"
    --source device:path-source --target device:path-source
)
check_selection_rejection(
    "source and target must both belong to scope site:verified"
    --source device:partial-source --target device:path-tail
)
check_selection_rejection(
    "declared failure not found: failure:missing"
    --source device:path-source --target device:path-tail --failure failure:missing
)

set(restricted_analyze
    analyze --model-root "${LTFA_SLICE2_ROOT}" --input restricted.ltfa
    --scope site:restricted --source restricted:a --target restricted:b
)
run_cli(analyze_restricted_default ${restricted_analyze} --format json)
run_cli(analyze_restricted_explicit
    ${restricted_analyze} --include-restricted --format json
)
if(NOT analyze_restricted_default_result EQUAL 4 OR
   NOT analyze_restricted_default_error STREQUAL "" OR
   NOT analyze_restricted_explicit_result EQUAL 0 OR
   NOT analyze_restricted_explicit_error STREQUAL "")
    message(FATAL_ERROR "restricted analysis must require explicit inclusion")
endif()

run_cli(model_help_json_first help model --format json)
run_cli(model_help_json_second help model --format=json)
if(NOT model_help_json_first_result EQUAL 0 OR NOT model_help_json_second_result EQUAL 0 OR
   NOT model_help_json_first_error STREQUAL "" OR
   NOT model_help_json_second_error STREQUAL "" OR
   NOT model_help_json_first_output STREQUAL model_help_json_second_output)
    message(FATAL_ERROR "structured model help must be byte-stable")
endif()
string(JSON model_help_query GET "${model_help_json_first_output}" query command)
string(JSON model_help_count LENGTH "${model_help_json_first_output}" commands)
string(JSON model_help_effect GET "${model_help_json_first_output}" commands 0 effect)
string(JSON model_help_exit_count LENGTH
    "${model_help_json_first_output}" commands 0 exit_statuses
)
string(JSON model_help_flag_default_type TYPE
    "${model_help_json_first_output}" commands 0 options 0 default
)
string(JSON model_help_required_default_type TYPE
    "${model_help_json_first_output}" commands 0 options 1 default
)
string(JSON model_help_dot_view_name GET
    "${model_help_json_first_output}" commands 0 options 4 name
)
string(JSON model_help_dot_view_default GET
    "${model_help_json_first_output}" commands 0 options 4 default
)
string(JSON model_help_dot_view_requires GET
    "${model_help_json_first_output}" commands 0 options 4 requires 0
)
if(NOT model_help_query STREQUAL "model" OR NOT model_help_count EQUAL 1 OR
   NOT model_help_effect STREQUAL "read" OR NOT model_help_exit_count EQUAL 5 OR
   NOT model_help_flag_default_type STREQUAL "BOOLEAN" OR
   NOT model_help_required_default_type STREQUAL "NULL" OR
   NOT model_help_dot_view_name STREQUAL "dot-view" OR
   NOT model_help_dot_view_default STREQUAL "raw" OR
   NOT model_help_dot_view_requires STREQUAL "format=dot")
    message(FATAL_ERROR "structured model help is incomplete")
endif()

set(model_command
    model
    --model-root "${LTFA_VALID_ROOT}"
    --input main.ltfa
)

run_cli(json_first ${model_command} --format json)
run_cli(json_second ${model_command} --format json)
run_cli(json_equals
    model
    "--model-root=${LTFA_VALID_ROOT}"
    --input=main.ltfa
    --format=json
)
if(NOT json_first_result EQUAL 0 OR NOT json_second_result EQUAL 0 OR
   NOT json_equals_result EQUAL 0)
    message(FATAL_ERROR
        "JSON model command failed: "
        "${json_first_error}${json_second_error}${json_equals_error}"
    )
endif()
if(NOT json_first_error STREQUAL "" OR NOT json_second_error STREQUAL "" OR
   NOT json_equals_error STREQUAL "")
    message(FATAL_ERROR "successful JSON model command wrote stderr")
endif()
if(NOT json_first_output STREQUAL json_second_output OR
   NOT json_first_output STREQUAL json_equals_output)
    message(FATAL_ERROR "JSON model output changed between equivalent invocations")
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

run_cli(dot_raw_explicit ${model_command} --format dot --dot-view raw)
if(NOT dot_raw_explicit_result EQUAL 0 OR NOT dot_raw_explicit_error STREQUAL "" OR
   NOT dot_raw_explicit_output STREQUAL dot_first_output)
    message(FATAL_ERROR "explicit raw DOT view must match the compatible default byte-for-byte")
endif()

foreach(view topology logical protection power dependency overlay)
    run_cli(dot_semantic_${view}
        ${model_command} --format dot --dot-view "${view}"
    )
    if(NOT dot_semantic_${view}_result EQUAL 0 OR
       NOT dot_semantic_${view}_error STREQUAL "" OR
       NOT dot_semantic_${view}_output MATCHES "^digraph ltfa_diagram" OR
       NOT dot_semantic_${view}_output MATCHES
           "ltfa_schema=\"ltfa.diagram.dot/v1\".*view=\"${view}\"")
        message(FATAL_ERROR "semantic ${view} DOT view did not emit its stable contract")
    endif()
endforeach()

run_cli(resilience_analysis
    analyze --model-root "${LTFA_RESILIENCE_ROOT}" --input main.ltfa
    --scope site:resilience --source switch:1:p1 --target switch:2:p1
    --failure failure:link-1
)
string(FIND "${resilience_analysis_output}"
    "immediate-degraded:\n    - lag:uplink" resilience_degraded)
if(NOT resilience_analysis_result EQUAL 0 OR
   NOT resilience_analysis_error STREQUAL "" OR
   NOT resilience_analysis_output MATCHES "reachable: true" OR
   resilience_analysis_output MATCHES "relation:member_of" OR
   resilience_analysis_output MATCHES "id: stp:default" OR
   resilience_analysis_output MATCHES "alternatives-excluded-by-filter: true" OR
   resilience_degraded EQUAL -1)
    message(FATAL_ERROR
        "CLI assurance must not treat structural membership as forwarding connectivity")
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

run_cli(malformed_json
    model --model-root "${LTFA_INVALID_ROOT}" --input malformed.ltfa --format json
)
if(NOT malformed_json_result EQUAL 3 OR NOT malformed_json_error STREQUAL "")
    message(FATAL_ERROR "malformed JSON-mode input must return exit 3 on stdout")
endif()
string(JSON malformed_schema GET "${malformed_json_output}" schema)
string(JSON malformed_status GET "${malformed_json_output}" exit_status)
string(JSON malformed_code GET "${malformed_json_output}" diagnostics 0 code)
string(JSON malformed_file GET "${malformed_json_output}" diagnostics 0 source file)
string(JSON malformed_line GET "${malformed_json_output}" diagnostics 0 source line)
string(JSON malformed_next GET "${malformed_json_output}" actions 0 description)
string(JSON malformed_guide GET "${malformed_json_output}" actions 1 invocation)
string(JSON malformed_help GET "${malformed_json_output}" actions 2 invocation)
string(JSON malformed_action_count LENGTH "${malformed_json_output}" actions)
if(NOT malformed_schema STREQUAL "ltfa.cli-error/v1" OR NOT malformed_status EQUAL 3 OR
   NOT malformed_code STREQUAL "E_DSL_UNKNOWN_STATEMENT" OR
   NOT malformed_file STREQUAL "malformed.ltfa" OR NOT malformed_line EQUAL 2 OR
   NOT malformed_next STREQUAL
       "Correct the reported local input, then rerun ltfa model." OR
   NOT malformed_action_count EQUAL 3 OR
   NOT malformed_guide STREQUAL "ltfa guide errors dsl" OR
   NOT malformed_help STREQUAL "ltfa help model")
    message(FATAL_ERROR "malformed JSON-mode diagnostic contract is incorrect")
endif()

run_cli(malformed_human
    model --model-root "${LTFA_INVALID_ROOT}" --input malformed.ltfa --format human
)
if(NOT malformed_human_result EQUAL 3 OR NOT malformed_human_output STREQUAL "" OR
   NOT malformed_human_error MATCHES
       "^malformed.ltfa:2:1: error E_DSL_UNKNOWN_STATEMENT: unknown DSL statement" OR
   NOT malformed_human_error MATCHES
       "  next: Correct the reported local input, then rerun ltfa model[.]\n"
       OR NOT malformed_human_error MATCHES "  guide: ltfa guide errors dsl\n"
       OR NOT malformed_human_error MATCHES "  help: ltfa help model\n$")
    message(FATAL_ERROR "human malformed input must retain its stable source diagnostic")
endif()

run_cli(duplicate
    model --model-root "${LTFA_INVALID_ROOT}" --input duplicate.ltfa --format json
)
if(NOT duplicate_result EQUAL 4 OR NOT duplicate_error STREQUAL "")
    message(FATAL_ERROR "JSON-mode validation failure must return exit 4 on stdout")
endif()
string(JSON duplicate_schema GET "${duplicate_output}" schema)
string(JSON duplicate_status GET "${duplicate_output}" exit_status)
string(JSON duplicate_code GET "${duplicate_output}" diagnostics 0 code)
string(JSON duplicate_file GET "${duplicate_output}" diagnostics 0 source file)
string(JSON duplicate_related_count LENGTH "${duplicate_output}" diagnostics 0 related)
if(NOT duplicate_schema STREQUAL "ltfa.cli-error/v1" OR NOT duplicate_status EQUAL 4 OR
   NOT duplicate_code STREQUAL "E_ID_DUPLICATE" OR
   NOT duplicate_file STREQUAL "duplicate.ltfa" OR NOT duplicate_related_count EQUAL 1)
    message(FATAL_ERROR "JSON-mode validation diagnostic contract is incorrect")
endif()

run_cli(escape
    model --model-root "${LTFA_INVALID_ROOT}/escape-root" --input main.ltfa --format json
)
if(NOT escape_result EQUAL 3 OR NOT escape_error STREQUAL "")
    message(FATAL_ERROR "escaping include must fail as JSON-mode input")
endif()
string(JSON escape_status GET "${escape_output}" exit_status)
string(JSON escape_code GET "${escape_output}" diagnostics 0 code)
if(NOT escape_status EQUAL 3 OR NOT escape_code STREQUAL "E_INCLUDE_ESCAPE")
    message(FATAL_ERROR "escaping include JSON diagnostic is incorrect")
endif()

run_cli(unknown_command modle)
if(NOT unknown_command_result EQUAL 2 OR NOT unknown_command_output STREQUAL "" OR
   NOT unknown_command_error MATCHES
       "^LTCLI-001 error: unknown command modle\n  token: modle\n  suggestion: model\n"
       OR NOT unknown_command_error MATCHES "  help: ltfa help\n$")
    message(FATAL_ERROR "unknown command must provide a stable suggestion and help route")
endif()

run_cli(unknown_option
    model --model-root "${LTFA_VALID_ROOT}" --input main.ltfa --formt human
)
if(NOT unknown_option_result EQUAL 2 OR NOT unknown_option_output STREQUAL "" OR
   NOT unknown_option_error MATCHES
       "^LTCLI-001 error: unknown option --formt\n  token: --formt\n"
       OR NOT unknown_option_error MATCHES "  suggestion: --format\n"
       OR NOT unknown_option_error MATCHES "  help: ltfa help model\n$")
    message(FATAL_ERROR "unknown option must provide a stable suggestion and help route")
endif()

run_cli(unknown_option_json
    model --model-root "${LTFA_VALID_ROOT}" --input main.ltfa
    --formt human --format json
)
if(NOT unknown_option_json_result EQUAL 2 OR NOT unknown_option_json_error STREQUAL "")
    message(FATAL_ERROR "JSON-mode usage failure must return exit 2 on stdout")
endif()
string(JSON usage_schema GET "${unknown_option_json_output}" schema)
string(JSON usage_status GET "${unknown_option_json_output}" exit_status)
string(JSON usage_kind GET "${unknown_option_json_output}" usage kind)
string(JSON usage_token GET "${unknown_option_json_output}" usage token)
string(JSON usage_suggestion GET "${unknown_option_json_output}" usage suggestion)
string(JSON usage_code GET "${unknown_option_json_output}" diagnostics 0 code)
string(JSON usage_action_count LENGTH "${unknown_option_json_output}" actions)
string(JSON usage_retry_id GET "${unknown_option_json_output}" actions 0 id)
string(JSON usage_retry_description GET "${unknown_option_json_output}" actions 0 description)
string(JSON usage_help_invocation GET "${unknown_option_json_output}" actions 1 invocation)
if(NOT usage_schema STREQUAL "ltfa.cli-error/v1" OR NOT usage_status EQUAL 2 OR
   NOT usage_kind STREQUAL "unknown_option" OR NOT usage_token STREQUAL "--formt" OR
   NOT usage_suggestion STREQUAL "--format" OR NOT usage_code STREQUAL "LTCLI-001" OR
   NOT usage_action_count EQUAL 2 OR NOT usage_retry_id STREQUAL "correct-and-retry" OR
   NOT usage_retry_description STREQUAL
       "Replace --formt with --format, then rerun ltfa model." OR
   NOT usage_help_invocation STREQUAL "ltfa help model")
    message(FATAL_ERROR "JSON-mode usage diagnostic contract is incorrect")
endif()

run_cli(help_unknown_json help modle --format json)
if(NOT help_unknown_json_result EQUAL 2 OR NOT help_unknown_json_error STREQUAL "")
    message(FATAL_ERROR "structured help lookup failure must return exit 2 on stdout")
endif()
string(JSON help_unknown_schema GET "${help_unknown_json_output}" schema)
string(JSON help_unknown_operation GET "${help_unknown_json_output}" operation)
string(JSON help_unknown_suggestion GET "${help_unknown_json_output}" usage suggestion)
if(NOT help_unknown_schema STREQUAL "ltfa.cli-error/v1" OR
   NOT help_unknown_operation STREQUAL "help" OR
   NOT help_unknown_suggestion STREQUAL "model")
    message(FATAL_ERROR "structured help lookup recovery is incorrect")
endif()

run_cli(invalid_format
    model --model-root "${LTFA_VALID_ROOT}" --input main.ltfa --format yaml
)
if(NOT invalid_format_result EQUAL 2 OR NOT invalid_format_output STREQUAL "" OR
   NOT invalid_format_error MATCHES
       "^LTCLI-001 error: option --format must be one of human, json, or dot")
    message(FATAL_ERROR "invalid CLI enum must return a stable usage error")
endif()

run_cli(invalid_dot_view
    model --model-root "${LTFA_VALID_ROOT}" --input main.ltfa
    --format dot --dot-view map
)
if(NOT invalid_dot_view_result EQUAL 2 OR NOT invalid_dot_view_output STREQUAL "" OR
   NOT invalid_dot_view_error MATCHES
       "^LTCLI-001 error: option --dot-view must be one of raw, topology, logical, protection, power, dependency, or overlay")
    message(FATAL_ERROR "unknown DOT view must return a stable usage error")
endif()

run_cli(dot_view_without_dot
    model --model-root "${LTFA_VALID_ROOT}" --input main.ltfa
    --format human --dot-view topology
)
if(NOT dot_view_without_dot_result EQUAL 2 OR
   NOT dot_view_without_dot_output STREQUAL "" OR
   NOT dot_view_without_dot_error MATCHES
       "^LTCLI-001 error: option --dot-view requires --format dot")
    message(FATAL_ERROR "DOT view must reject non-DOT output with a stable dependency error")
endif()

run_cli(dot_view_json_conflict
    model --model-root "${LTFA_VALID_ROOT}" --input main.ltfa
    --format json --dot-view topology
)
if(NOT dot_view_json_conflict_result EQUAL 2 OR
   NOT dot_view_json_conflict_error STREQUAL "")
    message(FATAL_ERROR "JSON-mode DOT view dependency error must be emitted on stdout")
endif()
string(JSON dot_view_usage_kind GET "${dot_view_json_conflict_output}" usage kind)
string(JSON dot_view_usage_token GET "${dot_view_json_conflict_output}" usage token)
if(NOT dot_view_usage_kind STREQUAL "option_dependency" OR
   NOT dot_view_usage_token STREQUAL "--dot-view")
    message(FATAL_ERROR "structured DOT view dependency error is incomplete")
endif()

run_cli(missing_required model --input main.ltfa)
if(NOT missing_required_result EQUAL 2 OR NOT missing_required_output STREQUAL "" OR
   NOT missing_required_error MATCHES
       "^LTCLI-001 error: option --model-root is required")
    message(FATAL_ERROR "missing required option must return a stable usage error")
endif()

run_cli(duplicate_option
    model --model-root "${LTFA_VALID_ROOT}" --model-root "${LTFA_VALID_ROOT}"
    --input main.ltfa
)
if(NOT duplicate_option_result EQUAL 2 OR NOT duplicate_option_output STREQUAL "" OR
   NOT duplicate_option_error MATCHES
       "^LTCLI-001 error: option --model-root is not repeatable")
    message(FATAL_ERROR "duplicate option must return a stable usage error")
endif()

run_cli(extra_operand ${model_command} unexpected)
if(NOT extra_operand_result EQUAL 2 OR NOT extra_operand_output STREQUAL "" OR
   NOT extra_operand_error MATCHES
       "^LTCLI-001 error: unexpected operand unexpected")
    message(FATAL_ERROR "extra operand must return a stable usage error")
endif()

run_cli(help_conflict model --help --format human)
if(NOT help_conflict_result EQUAL 2 OR NOT help_conflict_output STREQUAL "" OR
   NOT help_conflict_error MATCHES
       "^LTCLI-001 error: option --help conflicts with other arguments")
    message(FATAL_ERROR "help conflict must return a stable usage error")
endif()

run_cli(absolute_input
    model --model-root "${LTFA_VALID_ROOT}" --input "${LTFA_VALID_ROOT}/main.ltfa"
)
if(NOT absolute_input_result EQUAL 2 OR NOT absolute_input_output STREQUAL "" OR
   NOT absolute_input_error MATCHES
       "^LTCLI-001 error: option --input must be relative to --model-root")
    message(FATAL_ERROR "CLI entry input must remain relative to the explicit model root")
endif()
