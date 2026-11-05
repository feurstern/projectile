# Reads the Projectile version from include/projectile/core/version.hpp, which is the single
# source of truth for the version number.
#
# Sets, in the calling scope:
#   PROJECTILE_HEADER_VERSION       "MAJOR.MINOR.PATCH"
#   PROJECTILE_HEADER_VERSION_FILE  absolute path of the header that was parsed
#
# In script mode it prints the version to standard output, for tools outside CMake such as the
# release workflow:
#   cmake -P cmake/ProjectileVersion.cmake

get_filename_component(
    PROJECTILE_HEADER_VERSION_FILE
    "${CMAKE_CURRENT_LIST_DIR}/../include/projectile/core/version.hpp"
    ABSOLUTE)

file(STRINGS "${PROJECTILE_HEADER_VERSION_FILE}" _projectile_version_lines
     REGEX "^#define PROJECTILE_VERSION_(MAJOR|MINOR|PATCH)[ \t]+[0-9]+")

set(_projectile_version_components "")
foreach(_projectile_component IN ITEMS MAJOR MINOR PATCH)
    set(_projectile_component_value "")
    foreach(_projectile_line IN LISTS _projectile_version_lines)
        if(_projectile_line MATCHES "^#define PROJECTILE_VERSION_${_projectile_component}[ \t]+([0-9]+)")
            set(_projectile_component_value "${CMAKE_MATCH_1}")
        endif()
    endforeach()
    if(_projectile_component_value STREQUAL "")
        message(FATAL_ERROR
            "Could not read PROJECTILE_VERSION_${_projectile_component} from "
            "${PROJECTILE_HEADER_VERSION_FILE}. Keep the '#define NAME value' form.")
    endif()
    list(APPEND _projectile_version_components "${_projectile_component_value}")
endforeach()
list(JOIN _projectile_version_components "." PROJECTILE_HEADER_VERSION)

unset(_projectile_version_lines)
unset(_projectile_version_components)
unset(_projectile_component)
unset(_projectile_component_value)
unset(_projectile_line)

if(CMAKE_SCRIPT_MODE_FILE STREQUAL CMAKE_CURRENT_LIST_FILE)
    execute_process(COMMAND "${CMAKE_COMMAND}" -E echo "${PROJECTILE_HEADER_VERSION}")
endif()
