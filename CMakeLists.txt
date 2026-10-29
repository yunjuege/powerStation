cmake_minimum_required(VERSION 3.20)

project(PfrGridTerminal VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(pfr_domain
    src/domain/Types.cpp
    src/domain/TopologyRegistry.cpp
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

add_library(pfr_compute
    src/compute/UnitRealtimeProcessor.cpp
    src/compute/ShardedComputeEngine.cpp
    src/compute/RealtimeAggregator.cpp
)

target_include_directories(pfr_compute
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(pfr_compute
    PUBLIC
        pfr_domain
)

if(MSVC)
    target_compile_options(
        pfr_compute
        PRIVATE /W4 /permissive- /utf-8
    )
else()
    target_compile_options(
        pfr_compute
        PRIVATE -Wall -Wextra -Wpedantic
    )
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

    add_executable(pfr_topology_tests
        tests/topology_registry_tests.cpp
    )

    target_link_libraries(pfr_topology_tests
        PRIVATE
            pfr_domain
    )

    if(MSVC)
        target_compile_options(
            pfr_topology_tests
            PRIVATE /W4 /permissive- /utf-8
        )
    else()
        target_compile_options(
            pfr_topology_tests
            PRIVATE -Wall -Wextra -Wpedantic
        )
    endif()

    add_test(
        NAME topology_registry_tests
        COMMAND pfr_topology_tests
    )

    set_tests_properties(
    domain_tests
    topology_registry_tests
    PROPERTIES
        TIMEOUT 10
    )

    add_executable(pfr_unit_realtime_tests
        tests/unit_realtime_processor_tests.cpp
    )

    target_link_libraries(pfr_unit_realtime_tests
        PRIVATE
            pfr_compute
    )

    if(MSVC)
        target_compile_options(
            pfr_unit_realtime_tests
            PRIVATE /W4 /permissive- /utf-8
        )
    else()
        target_compile_options(
            pfr_unit_realtime_tests
            PRIVATE -Wall -Wextra -Wpedantic
        )
    endif()

    add_test(
        NAME unit_realtime_processor_tests
        COMMAND pfr_unit_realtime_tests
    )

    set_tests_properties(
        unit_realtime_processor_tests
        PROPERTIES
            TIMEOUT 10
    )

    add_executable(pfr_sharded_compute_tests
        tests/sharded_compute_engine_tests.cpp
    )

    target_link_libraries(pfr_sharded_compute_tests
        PRIVATE
            pfr_compute
    )

    if(MSVC)
        target_compile_options(
            pfr_sharded_compute_tests
            PRIVATE /W4 /permissive- /utf-8
        )
    else()
        target_compile_options(
            pfr_sharded_compute_tests
            PRIVATE -Wall -Wextra -Wpedantic
        )
    endif()

    add_test(
        NAME sharded_compute_engine_tests
        COMMAND pfr_sharded_compute_tests
    )

    set_tests_properties(
        sharded_compute_engine_tests
        PROPERTIES
            TIMEOUT 15
    )

    add_executable(pfr_realtime_aggregator_tests
        tests/realtime_aggregator_tests.cpp
    )

    target_link_libraries(pfr_realtime_aggregator_tests
        PRIVATE
            pfr_compute
    )

    if(MSVC)
        target_compile_options(
            pfr_realtime_aggregator_tests
            PRIVATE /W4 /permissive- /utf-8
        )
    else()
        target_compile_options(
            pfr_realtime_aggregator_tests
            PRIVATE -Wall -Wextra -Wpedantic
        )
    endif()

    add_test(
        NAME realtime_aggregator_tests
        COMMAND pfr_realtime_aggregator_tests
    )

    set_tests_properties(
        realtime_aggregator_tests
        PROPERTIES
            TIMEOUT 10
    )
endif()
