foreach(required LTFA_STRACE LTFA_CLI LTFA_DIAGNOSIS_ROOT LTFA_TRACE)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "missing required setting ${required}")
    endif()
endforeach()

function(run_offline_onboarding label)
    set(trace "${LTFA_TRACE}.${label}")
    execute_process(
        COMMAND "${LTFA_STRACE}" -f -qq -e trace=network -o "${trace}"
            "${LTFA_CLI}" ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "offline ${label} failed: ${error}${output}")
    endif()
    file(READ "${trace}" trace_content)
    file(REMOVE "${trace}")
    string(STRIP "${trace_content}" trace_content)
    if(NOT trace_content STREQUAL "")
        message(FATAL_ERROR "${label} attempted a network syscall:\n${trace_content}")
    endif()
endfunction()

run_offline_onboarding(help help diagnose --format json)
run_offline_onboarding(guide guide observations fields --format json)
run_offline_onboarding(completion __completion --bash guide observations f)
run_offline_onboarding(diagnosis diagnose
    --model-root "${LTFA_DIAGNOSIS_ROOT}"
    --input main.ltfa
    --observations observations.jsonl
    --analysis-at 2030-01-01T00:10:30Z
    --format json
)
