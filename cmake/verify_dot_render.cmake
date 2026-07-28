foreach(required LTFA_DOT LTFA_DOT_PRODUCER LTFA_DOT_FILE LTFA_SVG_FILE)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "missing required setting ${required}")
    endif()
endforeach()

execute_process(
    COMMAND "${LTFA_DOT_PRODUCER}" --emit-dot
    RESULT_VARIABLE producer_result
    OUTPUT_FILE "${LTFA_DOT_FILE}"
    ERROR_VARIABLE producer_error
)
if(NOT producer_result EQUAL 0)
    message(FATAL_ERROR "DOT fixture producer failed: ${producer_error}")
endif()

execute_process(
    COMMAND "${LTFA_DOT}" -Tsvg "${LTFA_DOT_FILE}" -o "${LTFA_SVG_FILE}"
    RESULT_VARIABLE dot_result
    ERROR_VARIABLE dot_error
)
if(NOT dot_result EQUAL 0)
    message(FATAL_ERROR "Graphviz rejected LTFA DOT: ${dot_error}")
endif()

file(READ "${LTFA_SVG_FILE}" svg LIMIT 1024)
file(REMOVE "${LTFA_DOT_FILE}" "${LTFA_SVG_FILE}")
if(NOT svg MATCHES "<svg")
    message(FATAL_ERROR "Graphviz output did not contain an SVG document")
endif()
