foreach(required LTFA_CLI LTFA_EXAMPLE_ROOT LTFA_EXPECTED_VERSION)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "missing required setting ${required}")
    endif()
endforeach()

function(run_guide prefix)
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

run_guide(index guide)
if(NOT index_result EQUAL 0 OR NOT index_error STREQUAL "" OR
   NOT index_output MATCHES "^LTFA Guide" OR
   NOT index_output MATCHES "Topic: overview" OR
   NOT index_output MATCHES "syntax  LTFA DSL v1 document shape" OR
   NOT index_output MATCHES "workflows  Use validation before bounded analysis")
    message(FATAL_ERROR "guide index is incomplete: ${index_error}${index_output}")
endif()

run_guide(index_json guide --format json)
if(NOT index_json_result EQUAL 0 OR NOT index_json_error STREQUAL "")
    message(FATAL_ERROR "structured guide index failed: ${index_json_error}")
endif()
string(JSON index_schema GET "${index_json_output}" schema)
string(JSON index_contract GET "${index_json_output}" cli_contract_version)
string(JSON index_version GET "${index_json_output}" tool_version)
string(JSON index_topic TYPE "${index_json_output}" query topic)
string(JSON index_level GET "${index_json_output}" query level)
string(JSON index_topic_count LENGTH "${index_json_output}" topics)
string(JSON index_syntax_count LENGTH "${index_json_output}" syntax)
if(NOT index_schema STREQUAL "ltfa.language-guide/v1" OR NOT index_contract EQUAL 5 OR
   NOT index_version STREQUAL "${LTFA_EXPECTED_VERSION}" OR
   NOT index_topic STREQUAL "NULL" OR NOT index_level STREQUAL "index" OR
   NOT index_topic_count EQUAL 9 OR NOT index_syntax_count EQUAL 0)
    message(FATAL_ERROR "structured guide index contract is incorrect")
endif()

run_guide(syntax guide syntax --format json)
string(JSON syntax_topic GET "${syntax_output}" query topic)
string(JSON syntax_level GET "${syntax_output}" query level)
string(JSON syntax_quick_count LENGTH "${syntax_output}" quick_reference)
string(JSON syntax_notes_count LENGTH "${syntax_output}" notes)
string(JSON syntax_subtopic_count LENGTH "${syntax_output}" subtopics)
if(NOT syntax_result EQUAL 0 OR NOT syntax_error STREQUAL "" OR
   NOT syntax_topic STREQUAL "syntax" OR NOT syntax_level STREQUAL "quick" OR
   NOT syntax_quick_count EQUAL 3 OR NOT syntax_notes_count EQUAL 0 OR
   NOT syntax_subtopic_count EQUAL 0)
    message(FATAL_ERROR "quick syntax guidance is incorrect")
endif()

run_guide(statements_index guide statements --level index)
if(NOT statements_index_result EQUAL 0 OR NOT statements_index_error STREQUAL "" OR
   NOT statements_index_output MATCHES "Subtopics:" OR
   NOT statements_index_output MATCHES "relations  Connect physical or logical objects" OR
   NOT statements_index_output MATCHES "assurance  Declare availability requirements")
    message(FATAL_ERROR "statement subtopic index is incomplete")
endif()

run_guide(relations guide statements relations --level detail --format json)
string(JSON relations_topic GET "${relations_output}" query topic)
string(JSON relations_subtopic GET "${relations_output}" query subtopic)
string(JSON relations_level GET "${relations_output}" query level)
string(JSON relations_syntax_count LENGTH "${relations_output}" syntax)
string(JSON relations_note_count LENGTH "${relations_output}" notes)
string(JSON relations_example_count LENGTH "${relations_output}" examples)
if(NOT relations_result EQUAL 0 OR NOT relations_error STREQUAL "" OR
   NOT relations_topic STREQUAL "statements" OR NOT relations_subtopic STREQUAL "relations" OR
   NOT relations_level STREQUAL "detail" OR NOT relations_syntax_count EQUAL 3 OR
   NOT relations_note_count EQUAL 2 OR NOT relations_example_count EQUAL 1)
    message(FATAL_ERROR "detailed relation guidance is incorrect")
endif()

run_guide(minimal guide samples minimal --format json)
string(JSON minimal_level GET "${minimal_output}" query level)
string(JSON minimal_syntax_count LENGTH "${minimal_output}" syntax)
string(JSON minimal_reference GET "${minimal_output}" navigation language_reference)
if(NOT minimal_result EQUAL 0 OR NOT minimal_error STREQUAL "" OR
   NOT minimal_level STREQUAL "detail" OR NOT minimal_syntax_count EQUAL 5 OR
   NOT minimal_reference STREQUAL "share/doc/ltfa/docs/language/ltfa-dsl-v1.md")
    message(FATAL_ERROR "minimal sample guidance is incorrect")
endif()

run_guide(observation_fields guide observations fields --format json)
string(JSON observation_domain GET "${observation_fields_output}" domain)
string(JSON observation_schema GET
    "${observation_fields_output}" observation_contract schema)
string(JSON observation_required_count LENGTH
    "${observation_fields_output}" observation_contract required_fields)
string(JSON observation_optional_count LENGTH
    "${observation_fields_output}" observation_contract optional_fields)
string(JSON observation_kind_count LENGTH
    "${observation_fields_output}" observation_contract kinds)
string(JSON observation_outcome_count LENGTH
    "${observation_fields_output}" observation_contract outcomes)
string(JSON observation_reference GET
    "${observation_fields_output}" navigation observation_reference)
if(NOT observation_fields_result EQUAL 0 OR NOT observation_fields_error STREQUAL "" OR
   NOT observation_domain STREQUAL "observation-data" OR
   NOT observation_schema STREQUAL "ltfa.observation/v1" OR
   NOT observation_required_count EQUAL 8 OR NOT observation_optional_count EQUAL 8 OR
   NOT observation_kind_count EQUAL 5 OR NOT observation_outcome_count EQUAL 5 OR
   NOT observation_reference STREQUAL
       "share/doc/ltfa/docs/observations/ltfa-observation-v1.md")
    message(FATAL_ERROR "observation field guidance is incorrect")
endif()

run_guide(diagnosis_sample guide samples diagnosis --format json)
string(JSON diagnosis_sample_domain GET "${diagnosis_sample_output}" domain)
string(JSON diagnosis_sample_path GET
    "${diagnosis_sample_output}" navigation diagnosis_sample)
if(NOT diagnosis_sample_result EQUAL 0 OR NOT diagnosis_sample_error STREQUAL "" OR
   NOT diagnosis_sample_domain STREQUAL "observation-data" OR
   NOT diagnosis_sample_path STREQUAL "share/doc/ltfa/examples/diagnosis")
    message(FATAL_ERROR "diagnosis sample guidance is incorrect")
endif()

run_guide(unknown_topic guide statments)
if(NOT unknown_topic_result EQUAL 2 OR NOT unknown_topic_output STREQUAL "" OR
   NOT unknown_topic_error MATCHES "^LTCLI-001 error: unknown guide topic statments" OR
   NOT unknown_topic_error MATCHES "suggestion: statements" OR
   NOT unknown_topic_error MATCHES "guide: ltfa guide" OR
   NOT unknown_topic_error MATCHES "help: ltfa help guide")
    message(FATAL_ERROR "unknown guide topic navigation is incorrect")
endif()

run_guide(unknown_subtopic guide statements reltions --format json)
string(JSON unknown_schema GET "${unknown_subtopic_output}" schema)
string(JSON unknown_kind GET "${unknown_subtopic_output}" usage kind)
string(JSON unknown_suggestion GET "${unknown_subtopic_output}" usage suggestion)
string(JSON unknown_action_count LENGTH "${unknown_subtopic_output}" actions)
string(JSON unknown_guide_action GET "${unknown_subtopic_output}" actions 1 invocation)
string(JSON unknown_help_action GET "${unknown_subtopic_output}" actions 2 invocation)
if(NOT unknown_subtopic_result EQUAL 2 OR NOT unknown_subtopic_error STREQUAL "" OR
   NOT unknown_schema STREQUAL "ltfa.cli-error/v1" OR
   NOT unknown_kind STREQUAL "unknown_guide_subtopic" OR
   NOT unknown_suggestion STREQUAL "relations" OR NOT unknown_action_count EQUAL 3 OR
   NOT unknown_guide_action STREQUAL "ltfa guide statements" OR
   NOT unknown_help_action STREQUAL "ltfa help guide")
    message(FATAL_ERROR "unknown guide subtopic JSON navigation is incorrect")
endif()

run_guide(minimal_model model --model-root "${LTFA_EXAMPLE_ROOT}/minimal" --input main.ltfa)
if(NOT minimal_model_result EQUAL 0 OR NOT minimal_model_error STREQUAL "" OR
   NOT minimal_model_output MATCHES "entities: 2" OR
   NOT minimal_model_output MATCHES "relations: 1")
    message(FATAL_ERROR "minimal normative sample is not a valid model")
endif()

run_guide(resilience_model model --model-root "${LTFA_EXAMPLE_ROOT}/resilience" --input main.ltfa)
if(NOT resilience_model_result EQUAL 0 OR NOT resilience_model_error STREQUAL "" OR
   NOT resilience_model_output MATCHES "entities: 6" OR
   NOT resilience_model_output MATCHES "relations: 10")
    message(FATAL_ERROR "resilience normative sample is not a valid model")
endif()

run_guide(diagnosis_model model --model-root "${LTFA_EXAMPLE_ROOT}/diagnosis" --input main.ltfa)
if(NOT diagnosis_model_result EQUAL 0 OR NOT diagnosis_model_error STREQUAL "" OR
   NOT diagnosis_model_output MATCHES "entities: 3" OR
   NOT diagnosis_model_output MATCHES "probes: 1")
    message(FATAL_ERROR "diagnosis sample is not a valid model")
endif()

run_guide(diagnosis_run diagnose --model-root "${LTFA_EXAMPLE_ROOT}/diagnosis" --input main.ltfa
    --observations observations.jsonl --analysis-at 2030-01-01T00:10:30Z --format json)
string(JSON diagnosis_schema ERROR_VARIABLE diagnosis_json_error GET "${diagnosis_run_output}" schema)
if(NOT diagnosis_run_result EQUAL 0 OR NOT diagnosis_run_error STREQUAL "" OR
   diagnosis_json_error OR NOT diagnosis_schema STREQUAL "ltfa.diagnosis-cli/v1")
    message(FATAL_ERROR "diagnosis sample did not complete: ${diagnosis_json_error}")
endif()
