foreach(required
        LTFA_BUILD_DIR
        LTFA_INSTALL_PREFIX
        LTFA_INSTALL_BINDIR
        LTFA_INSTALL_DOCDIR
        LTFA_EXPECTED_VERSION
        LTFA_MODEL_ROOT
        LTFA_SLICE2_ROOT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "missing required setting ${required}")
    endif()
endforeach()

if(NOT DEFINED LTFA_HISTORY_ROOT OR LTFA_HISTORY_ROOT STREQUAL "")
    message(FATAL_ERROR "missing required setting LTFA_HISTORY_ROOT")
endif()

if(IS_ABSOLUTE "${LTFA_INSTALL_BINDIR}" OR IS_ABSOLUTE "${LTFA_INSTALL_DOCDIR}")
    message(FATAL_ERROR "install smoke test requires relative install directories")
endif()

file(REMOVE_RECURSE "${LTFA_INSTALL_PREFIX}")

set(install_command
    "${CMAKE_COMMAND}"
    --install "${LTFA_BUILD_DIR}"
    --prefix "${LTFA_INSTALL_PREFIX}"
)
if(DEFINED LTFA_INSTALL_CONFIG AND NOT LTFA_INSTALL_CONFIG STREQUAL "")
    list(APPEND install_command --config "${LTFA_INSTALL_CONFIG}")
endif()

execute_process(
    COMMAND ${install_command}
    RESULT_VARIABLE install_result
    OUTPUT_VARIABLE install_output
    ERROR_VARIABLE install_error
)
if(NOT install_result EQUAL 0)
    message(FATAL_ERROR
        "install failed with status ${install_result}\n${install_output}${install_error}"
    )
endif()

set(installed_cli
    "${LTFA_INSTALL_PREFIX}/${LTFA_INSTALL_BINDIR}/ltfa${LTFA_EXECUTABLE_SUFFIX}"
)
if(NOT EXISTS "${installed_cli}")
    message(FATAL_ERROR "installed CLI not found: ${installed_cli}")
endif()

foreach(document LICENSE README.md RELEASE_NOTES.md)
    set(installed_document
        "${LTFA_INSTALL_PREFIX}/${LTFA_INSTALL_DOCDIR}/${document}"
    )
    if(NOT EXISTS "${installed_document}")
        message(FATAL_ERROR "installed document not found: ${installed_document}")
    endif()
endforeach()

set(installed_development
    "${LTFA_INSTALL_PREFIX}/${LTFA_INSTALL_DOCDIR}/docs/development.md"
)
if(NOT EXISTS "${installed_development}")
    message(FATAL_ERROR
        "installed development guide not found: ${installed_development}"
    )
endif()

execute_process(
    COMMAND "${installed_cli}" --version
    RESULT_VARIABLE version_result
    OUTPUT_VARIABLE version_output
    ERROR_VARIABLE version_error
)
if(NOT version_result EQUAL 0 OR
   NOT version_output STREQUAL "ltfa ${LTFA_EXPECTED_VERSION}\n" OR
   NOT version_error STREQUAL "")
    message(FATAL_ERROR
        "installed version check failed: status=${version_result}\n"
        "stdout=${version_output}\nstderr=${version_error}"
    )
endif()

execute_process(
    COMMAND "${installed_cli}" --help
    RESULT_VARIABLE help_result
    OUTPUT_VARIABLE help_output
    ERROR_VARIABLE help_error
)
if(NOT help_result EQUAL 0 OR
   NOT help_output MATCHES "ltfa model --model-root" OR
   NOT help_error STREQUAL "")
    message(FATAL_ERROR
        "installed help check failed: status=${help_result}\n"
        "stdout=${help_output}\nstderr=${help_error}"
    )
endif()

execute_process(
    COMMAND
        "${installed_cli}"
        model
        --model-root "${LTFA_MODEL_ROOT}"
        --input main.ltfa
        --format json
    RESULT_VARIABLE model_result
    OUTPUT_VARIABLE model_output
    ERROR_VARIABLE model_error
)
if(NOT model_result EQUAL 0 OR NOT model_error STREQUAL "")
    message(FATAL_ERROR
        "installed model check failed: status=${model_result}\n"
        "stdout=${model_output}\nstderr=${model_error}"
    )
endif()

string(JSON model_schema ERROR_VARIABLE model_json_error GET "${model_output}" schema)
if(model_json_error OR NOT model_schema STREQUAL "ltfa.model/v1")
    message(FATAL_ERROR
        "installed model output is not ltfa.model/v1: ${model_json_error}"
    )
endif()

execute_process(
    COMMAND
        "${installed_cli}"
        model
        --model-root "${LTFA_MODEL_ROOT}"
        --input main.ltfa
        --format dot
        --dot-view topology
    RESULT_VARIABLE semantic_dot_result
    OUTPUT_VARIABLE semantic_dot_output
    ERROR_VARIABLE semantic_dot_error
)
if(NOT semantic_dot_result EQUAL 0 OR
   NOT semantic_dot_error STREQUAL "" OR
   NOT semantic_dot_output MATCHES "^digraph ltfa_diagram" OR
   NOT semantic_dot_output MATCHES
       "ltfa_schema=\"ltfa.diagram.dot/v1\".*view=\"topology\"")
    message(FATAL_ERROR
        "installed semantic DOT check failed: status=${semantic_dot_result}\n"
        "stdout=${semantic_dot_output}\nstderr=${semantic_dot_error}"
    )
endif()

execute_process(
    COMMAND
        "${installed_cli}"
        analyze
        --model-root "${LTFA_SLICE2_ROOT}"
        --input main.ltfa
        --scope site:verified
        --source device:path-source
        --target device:path-tail
        --failure failure:breaker
        --format json
    RESULT_VARIABLE analyze_result
    OUTPUT_VARIABLE analyze_output
    ERROR_VARIABLE analyze_error
)
if(NOT analyze_result EQUAL 0 OR NOT analyze_error STREQUAL "")
    message(FATAL_ERROR
        "installed analyze check failed: status=${analyze_result}\n"
        "stdout=${analyze_output}\nstderr=${analyze_error}"
    )
endif()

string(JSON analyze_schema ERROR_VARIABLE analyze_json_error
    GET "${analyze_output}" schema
)
string(JSON analyze_result_value GET "${analyze_output}" result)
string(JSON analyze_reachable GET "${analyze_output}" path reachable)
string(JSON analyze_spof_count LENGTH "${analyze_output}" spof findings)
string(JSON analyze_event_count LENGTH "${analyze_output}" propagation events)
if(analyze_json_error OR
   NOT analyze_schema STREQUAL "ltfa.assurance/v1" OR
   NOT analyze_result_value STREQUAL "complete" OR
   NOT analyze_reachable OR
   NOT analyze_spof_count EQUAL 3 OR
   NOT analyze_event_count EQUAL 4)
    message(FATAL_ERROR
        "installed analyze output does not satisfy the release contract: "
        "${analyze_json_error}"
    )
endif()

execute_process(
    COMMAND "${installed_cli}" cuts --model-root "${LTFA_SLICE2_ROOT}" --input main.ltfa
        --scope site:verified --source device:path-source --target device:path-target --format json
    RESULT_VARIABLE cuts_result OUTPUT_VARIABLE cuts_output ERROR_VARIABLE cuts_error
)
string(JSON cuts_schema ERROR_VARIABLE cuts_json_error GET "${cuts_output}" schema)
if(NOT cuts_result EQUAL 0 OR NOT cuts_error STREQUAL "" OR cuts_json_error OR
   NOT cuts_schema STREQUAL "ltfa.service-cuts/v1")
    message(FATAL_ERROR "installed cuts check failed: ${cuts_error}${cuts_json_error}")
endif()

execute_process(
    COMMAND "${installed_cli}" diagnose --model-root "${LTFA_HISTORY_ROOT}" --input main.ltfa
        --observations observations.jsonl --analysis-at 2030-01-01T00:10:30Z --format json
    RESULT_VARIABLE diagnosis_result OUTPUT_VARIABLE diagnosis_output ERROR_VARIABLE diagnosis_error
)
string(JSON diagnosis_schema ERROR_VARIABLE diagnosis_json_error GET "${diagnosis_output}" schema)
if(NOT diagnosis_result EQUAL 0 OR NOT diagnosis_error STREQUAL "" OR diagnosis_json_error OR
   NOT diagnosis_schema STREQUAL "ltfa.diagnosis-cli/v1")
    message(FATAL_ERROR "installed diagnosis check failed: ${diagnosis_error}${diagnosis_json_error}")
endif()
