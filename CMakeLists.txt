cmake_minimum_required(VERSION 3.20)

project(
    ltfa
    VERSION 0.0.0
    DESCRIPTION "Lightweight multilayer topology and fault analyzer"
    LANGUAGES CXX
)

include(CTest)
include(GNUInstallDirs)

set(LTFA_SANITIZERS "" CACHE STRING
    "Semicolon-separated sanitizer list: address, undefined, or leak"
)
option(LTFA_BUILD_FUZZERS "Build parser fuzz targets" OFF)
set(LTFA_FUZZ_ENGINE "standalone" CACHE STRING
    "Fuzz engine: standalone or libfuzzer"
)
set_property(CACHE LTFA_FUZZ_ENGINE PROPERTY STRINGS standalone libfuzzer)

configure_file(
    cmake/version_config.hpp.in
    generated/ltfa/version_config.hpp
    @ONLY
)

function(ltfa_enable_warnings target)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    elseif(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX)
    endif()
endfunction()

add_library(ltfa_sanitizers INTERFACE)
if(LTFA_SANITIZERS)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "LTFA sanitizers require GCC or Clang")
    endif()
    set(ltfa_sanitizer_names ${LTFA_SANITIZERS})
    list(REMOVE_DUPLICATES ltfa_sanitizer_names)
    foreach(sanitizer IN LISTS ltfa_sanitizer_names)
        if(NOT sanitizer MATCHES "^(address|undefined|leak)$")
            message(FATAL_ERROR
                "Unsupported LTFA sanitizer '${sanitizer}'; use address, undefined, or leak"
            )
        endif()
    endforeach()
    list(JOIN ltfa_sanitizer_names "," ltfa_sanitizer_csv)
    target_compile_options(ltfa_sanitizers INTERFACE
        -fno-omit-frame-pointer
        "-fsanitize=${ltfa_sanitizer_csv}"
    )
    target_link_options(ltfa_sanitizers INTERFACE
        "-fsanitize=${ltfa_sanitizer_csv}"
    )
endif()

set(LTFA_CORE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltfa/dsl/include_resolver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltfa/dsl/lexer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltfa/dsl/parser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltfa/error.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltfa/model.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltfa/model_compiler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltfa/model_output.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ltfa/version.cpp
)

add_library(ltfa_core STATIC ${LTFA_CORE_SOURCES})
add_library(ltfa::core ALIAS ltfa_core)

target_include_directories(ltfa_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/generated
)
target_compile_features(ltfa_core PUBLIC cxx_std_17)
target_link_libraries(ltfa_core PUBLIC ltfa_sanitizers)
set_target_properties(ltfa_core PROPERTIES CXX_EXTENSIONS OFF)
ltfa_enable_warnings(ltfa_core)

add_executable(ltfa
    app/main.cpp
)
target_link_libraries(ltfa PRIVATE ltfa::core)
target_compile_features(ltfa PRIVATE cxx_std_17)
set_target_properties(ltfa PROPERTIES CXX_EXTENSIONS OFF)
ltfa_enable_warnings(ltfa)

install(TARGETS ltfa
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
