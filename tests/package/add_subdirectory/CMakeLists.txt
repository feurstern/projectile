# Embeds the Projectile source tree, as add_subdirectory() and FetchContent users do.
cmake_minimum_required(VERSION 3.20...4.4)
project(ProjectileSubdirectoryConsumer LANGUAGES CXX)

if(NOT PROJECTILE_SOURCE_TREE)
    message(FATAL_ERROR "PROJECTILE_SOURCE_TREE must point to the Projectile source tree")
endif()

add_subdirectory("${PROJECTILE_SOURCE_TREE}" projectile)

# An embedded Projectile must not build its tests (which would download Catch2) or add install
# rules unless the parent project asks for them.
if(PROJECTILE_BUILD_TESTS OR PROJECTILE_INSTALL OR TARGET projectile_unit_tests)
    message(FATAL_ERROR "Embedded Projectile enabled its tests or install rules by default")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/../consumer.cmake")
