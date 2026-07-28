cmake_minimum_required(VERSION 3.20)

project(
    ltfa
    VERSION 0.0.0
    DESCRIPTION "Lightweight multilayer topology and fault analyzer"
    LANGUAGES CXX
)

include(CTest)
include(GNUInstallDirs)

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

add_library(ltfa_core STATIC
    src/ltfa/dsl/include_resolver.cpp
    src/ltfa/dsl/lexer.cpp
    src/ltfa/dsl/parser.cpp
    src/ltfa/error.cpp
    src/ltfa/model.cpp
    src/ltfa/model_compiler.cpp
    src/ltfa/model_output.cpp
    src/ltfa/version.cpp
)
add_library(ltfa::core ALIAS ltfa_core)

target_include_directories(ltfa_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/generated
)
target_compile_features(ltfa_core PUBLIC cxx_std_17)
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
