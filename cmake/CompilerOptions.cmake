# Compiler-related helpers for Projectile's own targets.
#
# Nothing here reaches downstream consumers: warning flags are added as PRIVATE options per
# target, and clang-tidy is attached per target (never through the global CMAKE_CXX_CLANG_TIDY,
# which would also analyze third-party code such as Catch2).

include_guard(GLOBAL)

# Warns when the compiler is older than the oldest version Projectile is tested with in CI.
# Older compilers may still work but are unsupported.
function(projectile_check_compiler_version)
    set(minimum "")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(minimum 12)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set(minimum 16)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
        set(minimum 15)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        set(minimum 19.30) # Visual Studio 2022
    endif()

    if(NOT minimum STREQUAL "" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS minimum)
        message(WARNING
            "Projectile is tested with ${CMAKE_CXX_COMPILER_ID} ${minimum} or newer, but "
            "${CMAKE_CXX_COMPILER_VERSION} was found. The build may fail.")
    endif()
endfunction()

# Adds Projectile's warning flags to <target> as PRIVATE compile options.
# PROJECTILE_WARNINGS_AS_ERRORS turns the warnings into errors.
function(projectile_target_warnings target)
    if(MSVC)
        set(warnings
            /W4
            /permissive-
            /w14242 # conversion from 'type1' to 'type2', possible loss of data
            /w14254 # larger bit field assigned to a smaller bit field
            /w14263 # member function does not override any base class virtual function
            /w14265 # class has virtual functions, but its destructor is not virtual
            /w14287 # unsigned/negative constant mismatch
            /we4289 # loop control variable used outside the for-loop scope
            /w14296 # expression is always true or always false
            /w14311 # pointer truncation
            /w14545 # expression before comma evaluates to a function missing an argument list
            /w14546 # function call before comma missing an argument list
            /w14547 # operator before comma has no effect
            /w14549 # operator before comma has no effect
            /w14555 # expression has no effect
            /w14826 # sign-extending conversion
            /w14905 # wide string literal cast to LPSTR
            /w14906 # string literal cast to LPWSTR
            /w14928 # illegal copy-initialization
        )
        if(PROJECTILE_WARNINGS_AS_ERRORS)
            list(APPEND warnings /WX)
        endif()
    else()
        set(warnings
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow
            -Wconversion
            -Wsign-conversion
            -Wdouble-promotion
            -Wold-style-cast
            -Wcast-align
            -Wnon-virtual-dtor
            -Woverloaded-virtual
            -Wformat=2
            -Wimplicit-fallthrough
        )
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            list(APPEND warnings -Wduplicated-cond -Wduplicated-branches -Wlogical-op)
        endif()
        if(PROJECTILE_WARNINGS_AS_ERRORS)
            list(APPEND warnings -Werror)
        endif()
    endif()

    target_compile_options(${target} PRIVATE ${warnings})
endfunction()

# Runs clang-tidy on <target>'s sources as they compile, when PROJECTILE_ENABLE_CLANG_TIDY is ON.
# Only the Makefile and Ninja generators support this.
#
#   projectile_target_clang_tidy(<target> [CONFIG_FILE <file>])
#
# CONFIG_FILE overrides the usual .clang-tidy lookup, which is needed for generated sources that
# live outside the source tree.
function(projectile_target_clang_tidy target)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "CONFIG_FILE" "")
    if(NOT PROJECTILE_ENABLE_CLANG_TIDY)
        return()
    endif()

    find_program(PROJECTILE_CLANG_TIDY_EXECUTABLE NAMES clang-tidy REQUIRED)
    # GCC-only warning flags appear in the compile command when building with GCC; clang-tidy
    # would otherwise report them as unknown.
    set(command "${PROJECTILE_CLANG_TIDY_EXECUTABLE}" "--extra-arg=-Wno-unknown-warning-option")
    if(arg_CONFIG_FILE)
        list(APPEND command "--config-file=${arg_CONFIG_FILE}")
    endif()
    set_target_properties(${target} PROPERTIES CXX_CLANG_TIDY "${command}")
endfunction()
