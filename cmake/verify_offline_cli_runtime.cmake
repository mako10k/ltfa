foreach(required LTFA_STRACE LTFA_CLI LTFA_MODEL_ROOT LTFA_SLICE2_ROOT LTFA_TRACE)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "missing required setting ${required}")
    endif()
endforeach()

execute_process(
    COMMAND
        "${LTFA_STRACE}" -f -qq -e trace=network -o "${LTFA_TRACE}"
        "${LTFA_CLI}" model --model-root "${LTFA_MODEL_ROOT}" --input main.ltfa --format json
    RESULT_VARIABLE cli_result
    OUTPUT_VARIABLE cli_output
    ERROR_VARIABLE cli_error
)
if(NOT cli_result EQUAL 0)
    message(FATAL_ERROR "offline CLI integration failed: ${cli_error}")
endif()
if(NOT cli_output MATCHES "\"schema\":\"ltfa.model/v1\"")
    message(FATAL_ERROR "offline CLI integration did not produce the model schema")
endif()

file(READ "${LTFA_TRACE}" trace)
file(REMOVE "${LTFA_TRACE}")
string(STRIP "${trace}" trace)
if(NOT trace STREQUAL "")
    message(FATAL_ERROR "offline CLI attempted a network syscall:\n${trace}")
endif()

set(analyze_trace "${LTFA_TRACE}.analyze")
execute_process(
    COMMAND
        "${LTFA_STRACE}" -f -qq -e trace=network -o "${analyze_trace}"
        "${LTFA_CLI}" analyze
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
if(NOT analyze_result EQUAL 0)
    message(FATAL_ERROR "offline analyze integration failed: ${analyze_error}")
endif()
if(NOT analyze_output MATCHES "\"schema\":\"ltfa.assurance/v1\"")
    message(FATAL_ERROR "offline analyze integration did not produce the assurance schema")
endif()

file(READ "${analyze_trace}" analyze_trace_content)
file(REMOVE "${analyze_trace}")
string(STRIP "${analyze_trace_content}" analyze_trace_content)
if(NOT analyze_trace_content STREQUAL "")
    message(FATAL_ERROR
        "offline analyze attempted a network syscall:\n${analyze_trace_content}"
    )
endif()
