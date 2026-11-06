# Example programs. Each one is a small, self-contained program that uses only the public API.

set(CMAKE_CXX_EXTENSIONS OFF)

function(projectile_add_example name)
    add_executable(${name} ${name}/main.cpp)
    target_link_libraries(${name} PRIVATE Projectile::Projectile)
    projectile_target_warnings(${name})
    projectile_target_clang_tidy(${name})
    if(PROJECTILE_BUILD_TESTS)
        # Smoke test: the example must run to completion.
        add_test(NAME example.${name} COMMAND ${name})
        set_tests_properties(example.${name} PROPERTIES LABELS example)
    endif()
endfunction()

projectile_add_example(01_basic_projectile)
projectile_add_example(02_ballistic_drop)
