cmake_minimum_required(VERSION 3.20)

project(PfrGridTerminal VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(pfr_domain
    src/domain/Types.cpp
)

target_include_directories(pfr_domain
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(pfr_grid_terminal
    src/main.cpp
)

target_link_libraries(pfr_grid_terminal
    PRIVATE
        pfr_domain
)

if(MSVC)
    target_compile_options(pfr_domain PRIVATE /W4 /permissive- /utf-8)
    target_compile_options(pfr_grid_terminal PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(pfr_domain PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(pfr_grid_terminal PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)

if(BUILD_TESTING)
    add_executable(pfr_domain_tests
        tests/domain_tests.cpp
    )

    target_link_libraries(pfr_domain_tests
        PRIVATE
            pfr_domain
    )

    if(MSVC)
        target_compile_options(pfr_domain_tests PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(pfr_domain_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(
        NAME domain_tests
        COMMAND pfr_domain_tests
    )
endif()