# Package-consumption tests: a small downstream project is configured, built and run against
# Projectile in the two ways real projects use it:
#
#   package.find_package      installed package, find_package(Projectile CONFIG)
#   package.add_subdirectory  embedded source tree (also what FetchContent does)
#
# Each test drives a nested build with `ctest --build-and-test`, so it needs the same compiler,
# flags (e.g. sanitizers), generator and configuration as this build; those are forwarded below.

set(_consumer_options
    "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
    "-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}"
    "-DCMAKE_EXE_LINKER_FLAGS=${CMAKE_EXE_LINKER_FLAGS}"
    "-DCMAKE_SHARED_LINKER_FLAGS=${CMAKE_SHARED_LINKER_FLAGS}"
    "-DCMAKE_BUILD_TYPE=$<CONFIG>"
    "-DBUILD_SHARED_LIBS=${BUILD_SHARED_LIBS}"
    "-DPROJECTILE_EXPECTED_VERSION=${PROJECT_VERSION}")

set(_generator_options --build-generator "${CMAKE_GENERATOR}")
if(CMAKE_GENERATOR_PLATFORM)
    list(APPEND _generator_options --build-generator-platform "${CMAKE_GENERATOR_PLATFORM}")
endif()
if(CMAKE_GENERATOR_TOOLSET)
    list(APPEND _generator_options --build-generator-toolset "${CMAKE_GENERATOR_TOOLSET}")
endif()
if(CMAKE_MAKE_PROGRAM AND NOT CMAKE_GENERATOR MATCHES "^Visual Studio")
    list(APPEND _generator_options --build-makeprogram "${CMAKE_MAKE_PROGRAM}")
endif()

# Installed package. A fixture installs this build into a staging prefix first.
if(PROJECTILE_INSTALL)
    set(_staging_prefix "${CMAKE_CURRENT_BINARY_DIR}/staging")

    add_test(NAME package.install
             COMMAND "${CMAKE_COMMAND}" --install "${PROJECT_BINARY_DIR}" --prefix
                     "${_staging_prefix}" --config $<CONFIG>)
    set_tests_properties(package.install PROPERTIES FIXTURES_SETUP projectile_staged_install
                                                    LABELS package)

    add_test(
        NAME package.find_package
        COMMAND
            "${CMAKE_CTEST_COMMAND}" --build-and-test "${CMAKE_CURRENT_SOURCE_DIR}/find_package"
            "${CMAKE_CURRENT_BINARY_DIR}/find_package" ${_generator_options} --build-config
            $<CONFIG> --build-options ${_consumer_options} "-DCMAKE_PREFIX_PATH=${_staging_prefix}"
            --test-command "${CMAKE_CTEST_COMMAND}" -C $<CONFIG> --output-on-failure)
    set_tests_properties(package.find_package PROPERTIES FIXTURES_REQUIRED projectile_staged_install
                                                         LABELS package)
endif()

# Embedded source tree.
add_test(
    NAME package.add_subdirectory
    COMMAND
        "${CMAKE_CTEST_COMMAND}" --build-and-test "${CMAKE_CURRENT_SOURCE_DIR}/add_subdirectory"
        "${CMAKE_CURRENT_BINARY_DIR}/add_subdirectory" ${_generator_options} --build-config
        $<CONFIG> --build-options ${_consumer_options}
        "-DPROJECTILE_SOURCE_TREE=${PROJECT_SOURCE_DIR}" --test-command "${CMAKE_CTEST_COMMAND}"
        -C $<CONFIG> --output-on-failure)
set_tests_properties(package.add_subdirectory PROPERTIES LABELS package)
