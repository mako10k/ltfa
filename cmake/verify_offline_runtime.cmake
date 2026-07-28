foreach(required LTFA_STRACE LTFA_PROGRAM LTFA_TRACE)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "missing required setting ${required}")
    endif()
endforeach()

execute_process(
    COMMAND "${LTFA_STRACE}" -f -qq -e trace=network -o "${LTFA_TRACE}" "${LTFA_PROGRAM}"
    RESULT_VARIABLE program_result
    ERROR_VARIABLE program_error
)
if(NOT program_result EQUAL 0)
    message(FATAL_ERROR "offline integration program failed: ${program_error}")
endif()

file(READ "${LTFA_TRACE}" trace)
file(REMOVE "${LTFA_TRACE}")
string(STRIP "${trace}" trace)
if(NOT trace STREQUAL "")
    message(FATAL_ERROR "offline integration program attempted a network syscall:\n${trace}")
endif()
