# Consumes an installed Projectile through its CMake package, as documented in the README.
cmake_minimum_required(VERSION 3.20...4.4)
project(ProjectileFindPackageConsumer LANGUAGES CXX)

# EXACT also verifies that the installed ProjectileConfigVersion.cmake reports this version.
find_package(Projectile ${PROJECTILE_EXPECTED_VERSION} EXACT CONFIG REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/../consumer.cmake")
