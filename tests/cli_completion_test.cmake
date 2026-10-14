if(NOT DEFINED LTFA_CLI OR LTFA_CLI STREQUAL "")
    message(FATAL_ERROR "missing required setting LTFA_CLI")
endif()

function(run_completion prefix)
    execute_process(
        COMMAND "${LTFA_CLI}" __completion --bash ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(NOT result EQUAL 0 OR NOT error STREQUAL "")
        message(FATAL_ERROR
            "completion query ${prefix} failed: status=${result}\n"
            "stdout=${output}\nstderr=${error}"
        )
    endif()
    set("${prefix}_output" "${output}" PARENT_SCOPE)
endfunction()

function(require_line output value label)
    string(FIND "\n${output}" "\n${value}\n" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "${label} is missing ${value}: ${output}")
    endif()
endfunction()

function(reject_line output value label)
    string(FIND "\n${output}" "\n${value}\n" found)
    if(NOT found EQUAL -1)
        message(FATAL_ERROR "${label} unexpectedly contains ${value}: ${output}")
    endif()
endfunction()

run_completion(top)
if(NOT top_output MATCHES "^__ltfa_completion_mode=plain\n")
    message(FATAL_ERROR "top-level completion mode is not plain: ${top_output}")
endif()
foreach(value model analyze forwarding cuts diagnose help guide --help -h --version)
    require_line("${top_output}" "${value}" "top-level completion")
endforeach()
reject_line("${top_output}" "__completion" "top-level completion")

run_completion(help_target help m)
if(NOT help_target_output STREQUAL "__ltfa_completion_mode=plain\nmodel\n")
    message(FATAL_ERROR "help-target completion is incorrect: ${help_target_output}")
endif()

run_completion(guide_topic guide st)
if(NOT guide_topic_output STREQUAL "__ltfa_completion_mode=plain\nstatements\n")
    message(FATAL_ERROR "guide-topic completion is incorrect: ${guide_topic_output}")
endif()

run_completion(guide_subtopic guide statements r)
if(NOT guide_subtopic_output STREQUAL "__ltfa_completion_mode=plain\nrelations\n")
    message(FATAL_ERROR "guide-subtopic completion is incorrect: ${guide_subtopic_output}")
endif()

run_completion(observation_topic guide ob)
if(NOT observation_topic_output STREQUAL "__ltfa_completion_mode=plain\nobservations\n")
    message(FATAL_ERROR "observation-topic completion is incorrect: ${observation_topic_output}")
endif()

run_completion(observation_subtopic guide observations f)
if(NOT observation_subtopic_output STREQUAL "__ltfa_completion_mode=plain\nformat\nfields\n")
    message(FATAL_ERROR
        "observation-subtopic completion is incorrect: ${observation_subtopic_output}")
endif()

run_completion(diagnosis_sample guide samples d)
if(NOT diagnosis_sample_output STREQUAL "__ltfa_completion_mode=plain\ndiagnosis\n")
    message(FATAL_ERROR "diagnosis-sample completion is incorrect: ${diagnosis_sample_output}")
endif()

run_completion(guide_level guide syntax --level d)
if(NOT guide_level_output STREQUAL "__ltfa_completion_mode=plain\ndetail\n")
    message(FATAL_ERROR "guide-level completion is incorrect: ${guide_level_output}")
endif()

run_completion(model_options model --)
foreach(value --help --model-root --input --format --dot-view --include-restricted)
    require_line("${model_options_output}" "${value}" "model option completion")
endforeach()

run_completion(model_short_options model -)
require_line("${model_short_options_output}" "-h" "model short-option completion")

run_completion(enum_value model --format j)
if(NOT enum_value_output STREQUAL "__ltfa_completion_mode=plain\njson\n")
    message(FATAL_ERROR "enum completion is incorrect: ${enum_value_output}")
endif()

run_completion(inline_enum model --format=j)
if(NOT inline_enum_output STREQUAL
   "__ltfa_completion_mode=plain\n--format=json\n")
    message(FATAL_ERROR "inline enum completion is incorrect: ${inline_enum_output}")
endif()

run_completion(used_option model --format json --)
require_line("${used_option_output}" "--model-root" "used-option completion")
reject_line("${used_option_output}" "--format" "used-option completion")

run_completion(directory_mode model --model-root .)
if(NOT directory_mode_output STREQUAL "__ltfa_completion_mode=dir\n")
    message(FATAL_ERROR "directory completion mode is incorrect: ${directory_mode_output}")
endif()

run_completion(file_mode diagnose --observations o)
if(NOT file_mode_output STREQUAL "__ltfa_completion_mode=file\n")
    message(FATAL_ERROR "file completion mode is incorrect: ${file_mode_output}")
endif()

run_completion(opaque_value analyze --scope site)
if(NOT opaque_value_output STREQUAL "__ltfa_completion_mode=none\n")
    message(FATAL_ERROR "semantic ID completion must remain disabled: ${opaque_value_output}")
endif()

execute_process(
    COMMAND "${LTFA_CLI}" __completion --zsh
    RESULT_VARIABLE invalid_result
    OUTPUT_VARIABLE invalid_output
    ERROR_VARIABLE invalid_error
)
if(NOT invalid_result EQUAL 2 OR NOT invalid_output STREQUAL "" OR
   NOT invalid_error MATCHES "^LTCLI-001 error: unknown command __completion")
    message(FATAL_ERROR "private completion transport accepted an invalid protocol")
endif()
