foreach(required LTFA_SOURCE_DIR LTFA_CORE_ARCHIVE LTFA_CLI LTFA_NM)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "missing required setting ${required}")
    endif()
endforeach()

file(GLOB_RECURSE source_files LIST_DIRECTORIES false
    "${LTFA_SOURCE_DIR}/app/*.cpp"
    "${LTFA_SOURCE_DIR}/include/*.hpp"
    "${LTFA_SOURCE_DIR}/src/*.cpp"
    "${LTFA_SOURCE_DIR}/tests/*.cpp"
)

set(forbidden_source_api
    "(^|[^A-Za-z0-9_])(socket|connect|accept|bind|listen|send|recv|getaddrinfo|system|popen|fork|vfork|execve|execl|execv|posix_spawn|dlopen|getenv|secure_getenv)[ \t\r\n]*\\("
)
set(secret_assignment
    "(password|passwd|api[_-]?key|access[_-]?token|secret)[ \t]*="
)

foreach(source IN LISTS source_files)
    file(READ "${source}" contents)
    string(REGEX MATCH "${forbidden_source_api}" forbidden_match "${contents}")
    if(forbidden_match)
        message(FATAL_ERROR
            "offline boundary violation in ${source}: forbidden API ${forbidden_match}"
        )
    endif()

    string(TOLOWER "${contents}" lowercase_contents)
    string(REGEX MATCH "${secret_assignment}" secret_match "${lowercase_contents}")
    if(secret_match OR lowercase_contents MATCHES "-----begin [a-z ]*private key-----")
        message(FATAL_ERROR "test or source tree contains a credential-shaped literal: ${source}")
    endif()
endforeach()

set(forbidden_symbols
    socket connect accept bind listen send recv sendto recvfrom getaddrinfo
    system popen fork vfork execve execl execlp execv execvp posix_spawn
    dlopen getenv secure_getenv
)
foreach(artifact IN ITEMS "${LTFA_CORE_ARCHIVE}" "${LTFA_CLI}")
    execute_process(
        COMMAND "${LTFA_NM}" -u "${artifact}"
        RESULT_VARIABLE nm_result
        OUTPUT_VARIABLE undefined_symbols
        ERROR_VARIABLE nm_error
    )
    if(NOT nm_result EQUAL 0)
        message(FATAL_ERROR "nm failed for ${artifact}: ${nm_error}")
    endif()
    foreach(symbol IN LISTS forbidden_symbols)
        if(undefined_symbols MATCHES
           "(^|\n)[^\n]*[ \t]U[ \t]+_?${symbol}(@[^ \t\r\n]+)?(\n|$)")
            message(FATAL_ERROR "${artifact} imports forbidden symbol ${symbol}")
        endif()
    endforeach()
endforeach()
