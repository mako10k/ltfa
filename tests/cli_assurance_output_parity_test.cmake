foreach(required LTFA_CLI LTFA_SLICE2_ROOT)
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

function(json_scalar result json)
    set(path ${ARGN})
    string(JSON type TYPE "${json}" ${path})
    string(JSON value GET "${json}" ${path})
    if(type STREQUAL "BOOLEAN")
        if(value)
            set(value "true")
        else()
            set(value "false")
        endif()
    endif()
    set("${result}" "${value}" PARENT_SCOPE)
endfunction()

function(build_array_block result json indent label)
    set(path ${ARGN})
    string(JSON count LENGTH "${json}" ${path})
    set(block "${indent}${label}:\n")
    if(count EQUAL 0)
        string(APPEND block "${indent}  - none\n")
    else()
        math(EXPR last "${count} - 1")
        foreach(index RANGE 0 ${last})
            string(JSON item GET "${json}" ${path} ${index})
            string(APPEND block "${indent}  - ${item}\n")
        endforeach()
    endif()
    set("${result}" "${block}" PARENT_SCOPE)
endfunction()

function(build_spof_paths_block result json indent label finding_index path_name)
    string(JSON count LENGTH "${json}" spof findings ${finding_index} ${path_name})
    set(block "${indent}${label}:\n")
    if(count EQUAL 0)
        string(APPEND block "${indent}  - none\n")
    else()
        math(EXPR last "${count} - 1")
        foreach(index RANGE 0 ${last})
            json_scalar(
                target
                "${json}"
                spof findings ${finding_index} ${path_name} ${index} target
            )
            build_array_block(
                objects
                "${json}"
                "${indent}    "
                objects
                spof findings ${finding_index} ${path_name} ${index} objects
            )
            build_array_block(
                relations
                "${json}"
                "${indent}    "
                relations
                spof findings ${finding_index} ${path_name} ${index} relations
            )
            string(APPEND block "${indent}  - target: ${target}\n${objects}${relations}")
        endforeach()
    endif()
    set("${result}" "${block}" PARENT_SCOPE)
endfunction()

function(build_expected_human output_var json)
    foreach(field schema operation scope source target restricted_output)
        json_scalar("${field}" "${json}" "${field}")
    endforeach()
    json_scalar(analysis_result "${json}" result)
    build_array_block(model_inputs "${json}" "" model-inputs model_inputs)

    string(CONCAT output
        "result: ${analysis_result}\n"
        "schema: ${schema}\n"
        "operation: ${operation}\n"
        "focus: ${source} -> ${target}\n"
        "scope: ${scope}\n"
        "restricted-output: ${restricted_output}\n"
        "${model_inputs}"
    )

    foreach(field status reachable contains_unknown_availability)
        json_scalar("path_${field}" "${json}" path "${field}")
    endforeach()
    json_scalar(path_completeness "${json}" path qualification completeness)
    json_scalar(path_evidence_quality "${json}" path qualification evidence_quality)
    json_scalar(path_visited_objects "${json}" path usage visited_objects)
    json_scalar(path_examined_relations "${json}" path usage examined_relations)
    build_array_block(path_objects "${json}" "  " objects path objects)
    build_array_block(path_relations "${json}" "  " relations path relations)
    string(APPEND output
        "path:\n"
        "  status: ${path_status}\n"
        "  reachable: ${path_reachable}\n"
        "  completeness: ${path_completeness}\n"
        "  evidence-quality: ${path_evidence_quality}\n"
        "  contains-unknown: ${path_contains_unknown_availability}\n"
        "${path_objects}"
        "${path_relations}"
        "  usage: visited-objects=${path_visited_objects}"
        " examined-relations=${path_examined_relations}\n"
    )

    string(JSON structural_type TYPE "${json}" structural)
    if(structural_type STREQUAL "NULL")
        string(APPEND output "structural:\n  status: not-applicable\n")
    else()
        foreach(field status contains_unknown_availability alternatives_excluded_by_filter)
            json_scalar("structural_${field}" "${json}" structural "${field}")
        endforeach()
        json_scalar(
            structural_completeness
            "${json}"
            structural qualification completeness
        )
        json_scalar(
            structural_evidence_quality
            "${json}"
            structural qualification evidence_quality
        )
        json_scalar(
            structural_visited_objects
            "${json}"
            structural usage visited_objects
        )
        json_scalar(
            structural_examined_relations
            "${json}"
            structural usage examined_relations
        )
        build_array_block(
            articulation_points
            "${json}"
            "  "
            articulation-points
            structural articulation_points
        )
        build_array_block(bridges "${json}" "  " bridges structural bridges)
        string(APPEND output
            "structural:\n"
            "  status: ${structural_status}\n"
            "  completeness: ${structural_completeness}\n"
            "  evidence-quality: ${structural_evidence_quality}\n"
            "  contains-unknown: ${structural_contains_unknown_availability}\n"
            "  alternatives-excluded-by-filter: "
            "${structural_alternatives_excluded_by_filter}\n"
            "${articulation_points}"
            "${bridges}"
            "  usage: visited-objects=${structural_visited_objects}"
            " examined-relations=${structural_examined_relations}\n"
        )
    endif()

    json_scalar(spof_status "${json}" spof status)
    string(APPEND output "spof:\n  status: ${spof_status}\n")
    if(spof_status STREQUAL "not-applicable")
        json_scalar(spof_reason "${json}" spof reason)
        string(APPEND output "  reason: ${spof_reason}\n")
    endif()
    string(APPEND output "  findings:\n")
    string(JSON finding_count LENGTH "${json}" spof findings)
    if(finding_count EQUAL 0)
        string(APPEND output "    - none\n")
    else()
        math(EXPR finding_last "${finding_count} - 1")
        foreach(index RANGE 0 ${finding_last})
            foreach(field kind id)
                json_scalar(
                    "candidate_${field}"
                    "${json}"
                    spof findings ${index} candidate "${field}"
                )
            endforeach()
            foreach(field classification alternatives_excluded_by_filter)
                json_scalar(
                    "finding_${field}"
                    "${json}"
                    spof findings ${index} "${field}"
                )
            endforeach()
            json_scalar(
                finding_completeness
                "${json}"
                spof findings ${index} qualification completeness
            )
            json_scalar(
                finding_evidence_quality
                "${json}"
                spof findings ${index} qualification evidence_quality
            )
            build_array_block(
                affected_targets
                "${json}"
                "      "
                affected-targets
                spof findings ${index} affected_targets
            )
            build_array_block(
                affected_objects
                "${json}"
                "      "
                affected-objects
                spof findings ${index} affected_objects
            )
            build_array_block(
                common_cause_members
                "${json}"
                "      "
                common-cause-members
                spof findings ${index} common_cause_members
            )
            build_array_block(
                common_domains
                "${json}"
                "      "
                common-domains
                spof findings ${index} common_domains
            )
            build_spof_paths_block(
                baseline_paths
                "${json}"
                "      "
                baseline-paths
                ${index}
                baseline_paths
            )
            build_spof_paths_block(
                retained_paths
                "${json}"
                "      "
                retained-paths
                ${index}
                retained_paths
            )
            build_array_block(
                remaining_unknowns
                "${json}"
                "      "
                remaining-unknowns
                spof findings ${index} remaining_unknowns
            )
            string(APPEND output
                "    - candidate:\n"
                "        kind: ${candidate_kind}\n"
                "        id: ${candidate_id}\n"
                "      classification: ${finding_classification}\n"
                "${affected_targets}"
                "${affected_objects}"
                "${common_cause_members}"
                "${common_domains}"
                "${baseline_paths}"
                "${retained_paths}"
                "${remaining_unknowns}"
                "      completeness: ${finding_completeness}\n"
                "      evidence-quality: ${finding_evidence_quality}\n"
                "      alternatives-excluded-by-filter: "
                "${finding_alternatives_excluded_by_filter}\n"
            )
        endforeach()
    endif()
    if(NOT spof_status STREQUAL "not-applicable")
        foreach(field candidates_considered visited_objects examined_relations)
            json_scalar("spof_${field}" "${json}" spof usage "${field}")
        endforeach()
        string(APPEND output
            "  usage: candidates-considered=${spof_candidates_considered}"
            " visited-objects=${spof_visited_objects}"
            " examined-relations=${spof_examined_relations}\n"
        )
    endif()

    string(JSON propagation_type TYPE "${json}" propagation)
    string(APPEND output "propagation:\n")
    if(propagation_type STREQUAL "NULL")
        string(APPEND output "  status: not-requested\n")
    else()
        foreach(field status failure unresolved_failure_mode)
            json_scalar("propagation_${field}" "${json}" propagation "${field}")
        endforeach()
        json_scalar(
            propagation_completeness
            "${json}"
            propagation qualification completeness
        )
        json_scalar(
            propagation_evidence_quality
            "${json}"
            propagation qualification evidence_quality
        )
        string(APPEND output
            "  status: ${propagation_status}\n"
            "  failure: ${propagation_failure}\n"
            "  completeness: ${propagation_completeness}\n"
            "  evidence-quality: ${propagation_evidence_quality}\n"
            "  unresolved-failure-mode: ${propagation_unresolved_failure_mode}\n"
            "  events:\n"
        )
        string(JSON event_count LENGTH "${json}" propagation events)
        if(event_count EQUAL 0)
            string(APPEND output "    - none\n")
        else()
            math(EXPR event_last "${event_count} - 1")
            foreach(index RANGE 0 ${event_last})
                foreach(field effective_time_ms object previous_state state)
                    json_scalar(
                        "event_${field}"
                        "${json}"
                        propagation events ${index} "${field}"
                    )
                endforeach()
                json_scalar(
                    event_completeness
                    "${json}"
                    propagation events ${index} qualification completeness
                )
                json_scalar(
                    event_evidence_quality
                    "${json}"
                    propagation events ${index} qualification evidence_quality
                )
                string(APPEND output
                    "    - at-ms: ${event_effective_time_ms}\n"
                    "      object: ${event_object}\n"
                    "      previous-state: ${event_previous_state}\n"
                    "      state: ${event_state}\n"
                    "      completeness: ${event_completeness}\n"
                    "      evidence-quality: ${event_evidence_quality}\n"
                )
            endforeach()
        endif()
        set(object_json_names
            immediate_unavailable
            immediate_degraded
            delayed_unavailable
            delayed_degraded
            unaffected
            unknown
            detection_gaps
        )
        set(object_human_names
            immediate-unavailable
            immediate-degraded
            delayed-unavailable
            delayed-degraded
            unaffected
            unknown
            detection-gaps
        )
        foreach(index RANGE 0 6)
            list(GET object_json_names ${index} json_name)
            list(GET object_human_names ${index} human_name)
            build_array_block(
                propagation_objects
                "${json}"
                "  "
                "${human_name}"
                propagation objects "${json_name}"
            )
            string(APPEND output "${propagation_objects}")
        endforeach()
        foreach(field scheduled_events processed_events state_transitions)
            json_scalar("propagation_${field}" "${json}" propagation usage "${field}")
        endforeach()
        string(APPEND output
            "  usage: scheduled-events=${propagation_scheduled_events}"
            " processed-events=${propagation_processed_events}"
            " state-transitions=${propagation_state_transitions}\n"
        )
    endif()

    foreach(field
            max_candidates
            max_visited_objects
            max_examined_relations
            max_events
            max_state_transitions)
        json_scalar("budget_${field}" "${json}" budgets "${field}")
    endforeach()
    string(APPEND output
        "budgets:\n"
        "  max-candidates: ${budget_max_candidates}\n"
        "  max-visited-objects: ${budget_max_visited_objects}\n"
        "  max-examined-relations: ${budget_max_examined_relations}\n"
        "  max-events: ${budget_max_events}\n"
        "  max-state-transitions: ${budget_max_state_transitions}\n"
    )

    set("${output_var}" "${output}" PARENT_SCOPE)
endfunction()

function(assert_parity name)
    set(command ${ARGN})
    run_cli("${name}_json_first" ${command} --format json)
    run_cli("${name}_json_second" ${command} --format=json)
    run_cli("${name}_human_first" ${command} --format human)
    run_cli("${name}_human_second" ${command})

    foreach(invocation json_first json_second human_first human_second)
        if(NOT "${${name}_${invocation}_result}" EQUAL 0 OR
           NOT "${${name}_${invocation}_error}" STREQUAL "")
            message(FATAL_ERROR
                "${name} ${invocation} failed: "
                "status=${${name}_${invocation}_result} "
                "stderr=${${name}_${invocation}_error}"
            )
        endif()
    endforeach()
    if(NOT "${${name}_json_first_output}" STREQUAL "${${name}_json_second_output}")
        message(FATAL_ERROR "${name} JSON output is not byte-stable")
    endif()
    if(NOT "${${name}_human_first_output}" STREQUAL "${${name}_human_second_output}")
        message(FATAL_ERROR "${name} human output is not byte-stable")
    endif()

    build_expected_human(expected_human "${${name}_json_first_output}")
    if(NOT "${${name}_human_first_output}" STREQUAL "${expected_human}")
        message(FATAL_ERROR
            "${name} human output does not losslessly represent ltfa.assurance/v1"
        )
    endif()

    set("${name}_json" "${${name}_json_first_output}" PARENT_SCOPE)
endfunction()

set(base_command
    analyze
    --model-root "${LTFA_SLICE2_ROOT}"
    --input main.ltfa
    --scope site:verified
    --source device:path-source
    --target device:path-tail
    --failure failure:breaker
)

assert_parity(complete ${base_command})
json_scalar(complete_result "${complete_json}" result)
if(NOT complete_result STREQUAL "complete")
    message(FATAL_ERROR "complete parity fixture did not produce a complete result")
endif()

assert_parity(incomplete ${base_command} --max-candidates 1)
json_scalar(incomplete_result "${incomplete_json}" result)
if(NOT incomplete_result STREQUAL "incomplete")
    message(FATAL_ERROR "bounded parity fixture did not produce an incomplete result")
endif()

assert_parity(
    unreachable
    analyze
    --model-root "${LTFA_SLICE2_ROOT}"
    --input main.ltfa
    --scope site:verified
    --source device:path-source
    --target facility:breaker
)
string(JSON unreachable_structural_type TYPE "${unreachable_json}" structural)
json_scalar(unreachable_spof_status "${unreachable_json}" spof status)
json_scalar(unreachable_spof_reason "${unreachable_json}" spof reason)
string(JSON unreachable_propagation_type TYPE "${unreachable_json}" propagation)
if(NOT unreachable_structural_type STREQUAL "NULL" OR
   NOT unreachable_spof_status STREQUAL "not-applicable" OR
   NOT unreachable_spof_reason STREQUAL "target-unreachable" OR
   NOT unreachable_propagation_type STREQUAL "NULL")
    message(FATAL_ERROR "unreachable fixture did not exercise nullable parity")
endif()
