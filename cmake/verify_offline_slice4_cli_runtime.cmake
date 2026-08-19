foreach(required LTFA_STRACE LTFA_CLI LTFA_SLICE2_ROOT LTFA_HISTORY_ROOT LTFA_TRACE)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "missing required setting ${required}")
    endif()
endforeach()

execute_process(
    COMMAND "${LTFA_STRACE}" -f -qq -e trace=network -o "${LTFA_TRACE}"
        "${LTFA_CLI}" cuts --model-root "${LTFA_SLICE2_ROOT}" --input main.ltfa
        --scope site:verified --source device:path-source --target device:path-target --format json
    RESULT_VARIABLE cuts_result ERROR_VARIABLE cuts_error
)
if(NOT cuts_result EQUAL 0)
    message(FATAL_ERROR "offline cuts command failed: ${cuts_error}")
endif()
file(READ "${LTFA_TRACE}" cuts_trace)
if(NOT cuts_trace STREQUAL "")
    message(FATAL_ERROR "cuts command attempted a network syscall:\n${cuts_trace}")
endif()

execute_process(
    COMMAND "${LTFA_STRACE}" -f -qq -e trace=network -o "${LTFA_TRACE}"
        "${LTFA_CLI}" diagnose --model-root "${LTFA_HISTORY_ROOT}" --input main.ltfa
        --observations observations.jsonl --analysis-at 2030-01-01T00:10:30Z --format json
    RESULT_VARIABLE diagnosis_result ERROR_VARIABLE diagnosis_error
)
if(NOT diagnosis_result EQUAL 0)
    message(FATAL_ERROR "offline diagnose command failed: ${diagnosis_error}")
endif()
file(READ "${LTFA_TRACE}" diagnosis_trace)
file(REMOVE "${LTFA_TRACE}")
if(NOT diagnosis_trace STREQUAL "")
    message(FATAL_ERROR "diagnose command attempted a network syscall:\n${diagnosis_trace}")
endif()
