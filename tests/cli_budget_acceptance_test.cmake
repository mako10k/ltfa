foreach(required LTFA_CLI LTFA_SLICE2_ROOT)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "missing required setting ${required}")
    endif()
endforeach()

function(assert_budget_case name max_objects max_relations expected_result
         expected_total_objects expected_total_relations)
    execute_process(
        COMMAND "${LTFA_CLI}"
            analyze
            --model-root "${LTFA_SLICE2_ROOT}"
            --input main.ltfa
            --scope site:verified
            --source device:path-source
            --target device:path-tail
            --max-visited-objects "${max_objects}"
            --max-examined-relations "${max_relations}"
            --format json
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(NOT result EQUAL 0 OR NOT error STREQUAL "")
        message(FATAL_ERROR
            "${name}: bounded analysis must succeed without stderr: "
            "status=${result} stderr=${error}"
        )
    endif()

    string(JSON analysis_result GET "${output}" result)
    string(JSON configured_objects GET "${output}" budgets max_visited_objects)
    string(JSON configured_relations GET "${output}" budgets max_examined_relations)
    string(JSON path_objects GET "${output}" path usage visited_objects)
    string(JSON path_relations GET "${output}" path usage examined_relations)
    string(JSON spof_objects GET "${output}" spof usage visited_objects)
    string(JSON spof_relations GET "${output}" spof usage examined_relations)
    math(EXPR total_objects "${path_objects} + ${spof_objects}")
    math(EXPR total_relations "${path_relations} + ${spof_relations}")

    if(NOT analysis_result STREQUAL "${expected_result}" OR
       NOT configured_objects EQUAL max_objects OR
       NOT configured_relations EQUAL max_relations OR
       total_objects GREATER max_objects OR
       total_relations GREATER max_relations)
        message(FATAL_ERROR
            "${name}: cumulative work contract failed: "
            "result=${analysis_result} objects=${total_objects}/${max_objects} "
            "relations=${total_relations}/${max_relations}"
        )
    endif()
    if(NOT "${expected_total_objects}" STREQUAL "-" AND
       NOT total_objects EQUAL expected_total_objects)
        message(FATAL_ERROR
            "${name}: expected ${expected_total_objects} cumulative objects, "
            "got ${total_objects}"
        )
    endif()
    if(NOT "${expected_total_relations}" STREQUAL "-" AND
       NOT total_relations EQUAL expected_total_relations)
        message(FATAL_ERROR
            "${name}: expected ${expected_total_relations} cumulative relations, "
            "got ${total_relations}"
        )
    endif()
endfunction()

assert_budget_case(exact_complete 39 65 complete 39 65)
assert_budget_case(object_exhaustion 38 65 incomplete 38 -)
assert_budget_case(relation_exhaustion 39 64 incomplete - 64)
assert_budget_case(path_consumes_object_budget 5 65 incomplete 5 -)
assert_budget_case(path_consumes_relation_budget 39 7 incomplete - 7)
