cmake_minimum_required(VERSION 3.10)
project(PerfTest)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add compile options
add_compile_options(-Wall -Wextra -O3)

# Find required packages
find_package(Threads REQUIRED)

# Source files (start with minimal set for testing)
set(SOURCES
    main.cpp
    cpu_bench.cpp
    report.cpp
)

# Headers
set(HEADERS
    benchmark.h
    cpu_bench.h
    mem_bench.h
    disk_bench.h
    net_bench.h
    ipc_bench.h
    integrated_bench.h
    report.h
    utils.h
)

# Create executable
add_executable(perf_test ${SOURCES})

# Link libraries
target_link_libraries(perf_test 
    Threads::Threads
)

# Link rt library only on Linux (for shared memory)
if(UNIX AND NOT APPLE)
    target_link_libraries(perf_test rt)
endif()

# Add math library
target_link_libraries(perf_test m)