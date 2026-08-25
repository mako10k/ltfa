if(NOT DEFINED LTFA_CI_WORKFLOW)
    message(FATAL_ERROR "LTFA_CI_WORKFLOW is required")
endif()

if(NOT EXISTS "${LTFA_CI_WORKFLOW}")
    message(FATAL_ERROR "CI workflow does not exist: ${LTFA_CI_WORKFLOW}")
endif()

file(READ "${LTFA_CI_WORKFLOW}" workflow)

function(require_ci_text needle description)
    string(FIND "${workflow}" "${needle}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "CI contract is missing ${description}")
    endif()
endfunction()

require_ci_text("  push:" "the push trigger")
require_ci_text("  pull_request:" "the pull-request trigger")
require_ci_text("  workflow_dispatch:" "the manual trigger")
require_ci_text("permissions:\n  contents: read" "read-only repository permissions")
require_ci_text("runs-on: ubuntu-24.04" "the explicit Ubuntu runner")
require_ci_text(
    "uses: actions/checkout@3d3c42e5aac5ba805825da76410c181273ba90b1 # v7.0.1"
    "the immutable checkout pin"
)
require_ci_text("persist-credentials: false" "disabled persisted credentials")
require_ci_text("name: gcc-release" "GCC Release validation")
require_ci_text("name: clang-release" "Clang Release validation")
require_ci_text("preset: asan-ubsan" "ASan/UBSan validation")
require_ci_text("preset: lsan" "LSan validation")
require_ci_text("name: clang-fuzz-seeds" "fuzz-seed validation")
require_ci_text("graphviz strace" "DOT and runtime offline-boundary tools")
require_ci_text("cmake --preset \"$LTFA_PRESET\"" "preset configuration")
require_ci_text("cmake --build --preset \"$LTFA_PRESET\"" "preset build")
require_ci_text("ctest --preset \"$LTFA_PRESET\"" "preset tests")

if(workflow MATCHES "pull_request_target")
    message(FATAL_ERROR "CI contract must not use pull_request_target")
endif()

if(workflow MATCHES "secrets[.]")
    message(FATAL_ERROR "CI contract must not consume repository secrets")
endif()
