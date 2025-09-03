cmake_minimum_required(VERSION 2.8)
project(PerfTest)

# Set C++ standard (try C++17, fallback to C++14, then C++11)
if(CMAKE_VERSION VERSION_LESS "3.1")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # Try C++17, fallback to C++14, then C++11
        execute_process(COMMAND ${CMAKE_CXX_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
        if(GCC_VERSION VERSION_GREATER "7.0")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
        elseif(GCC_VERSION VERSION_GREATER "4.9")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
        else()
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
        endif()
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
    else()
        # Generic fallback
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    endif()
else()
    # Modern CMake
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()

# Add compile options
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O3")

# Find required packages
find_package(Threads REQUIRED)

# Source files
set(SOURCES
    main.cpp
    cpu_bench.cpp
    mem_bench.cpp
    disk_bench.cpp
    net_bench.cpp
    ipc_bench.cpp
    integrated_bench.cpp
    report.cpp
)

# Headers (for IDE support)
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

# Link libraries (CMake 2.8 compatible)
target_link_libraries(perf_test ${CMAKE_THREAD_LIBS_INIT})

# Link rt library only on Linux (for shared memory)
if(UNIX AND NOT APPLE)
    target_link_libraries(perf_test rt)
endif()

# Add math library
target_link_libraries(perf_test m)