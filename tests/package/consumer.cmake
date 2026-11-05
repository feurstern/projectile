# Shared by the consumer projects in this directory. Expects the Projectile::Projectile target
# to exist and PROJECTILE_EXPECTED_VERSION to hold the version under test.

if(NOT PROJECTILE_EXPECTED_VERSION)
    message(FATAL_ERROR "PROJECTILE_EXPECTED_VERSION must be set")
endif()

# Projectile's warning flags are private to Projectile and must never reach its consumers.
get_target_property(_projectile_compile_options Projectile::Projectile INTERFACE_COMPILE_OPTIONS)
if(_projectile_compile_options)
    message(FATAL_ERROR
        "Projectile::Projectile passes compile options to consumers: ${_projectile_compile_options}")
endif()

add_executable(consumer "${CMAKE_CURRENT_LIST_DIR}/consumer.cpp")
# Linking is all a consumer should need: include paths, C++20 and (for shared builds)
# PROJECTILE_SHARED all come from the target.
target_link_libraries(consumer PRIVATE Projectile::Projectile)
# Like most projects, this consumer does not use C++20 modules. Without this, CMake 3.28+ scans
# C++20 sources for module imports, which with Clang requires clang-scan-deps (missing from
# Ubuntu's clang-NN packages). See "Troubleshooting" in the README.
set_target_properties(consumer PROPERTIES CXX_SCAN_FOR_MODULES OFF)
target_compile_definitions(consumer PRIVATE
                           "CONSUMER_EXPECTED_VERSION=\"${PROJECTILE_EXPECTED_VERSION}\"")

# Windows has no rpath: a shared Projectile must sit next to the executable to be found.
get_target_property(_projectile_type Projectile::Projectile TYPE)
if(WIN32 AND _projectile_type STREQUAL "SHARED_LIBRARY")
    add_custom_command(
        TARGET consumer
        POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:Projectile::Projectile>"
                "$<TARGET_FILE_DIR:consumer>")
endif()

enable_testing()
add_test(NAME consumer COMMAND consumer)
