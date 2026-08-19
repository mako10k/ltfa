foreach(required LTFA_CUT_TEST LTFA_RECOMMENDATION_TEST)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "missing required setting ${required}")
    endif()
endforeach()

foreach(program "${LTFA_CUT_TEST}" "${LTFA_RECOMMENDATION_TEST}")
    execute_process(
        COMMAND "${program}"
        RESULT_VARIABLE program_result
        ERROR_VARIABLE program_error
    )
    if(NOT program_result EQUAL 0)
        message(FATAL_ERROR "Slice 4 acceptance program failed: ${program_error}")
    endif()
endforeach()
