cmake_minimum_required(VERSION 2.8)
project(PerfTest)

# Define _GNU_SOURCE before any includes for Linux CPU affinity functions
if(UNIX AND NOT APPLE)
    add_definitions(-D_GNU_SOURCE)
endif()

# Set C++ standard with version detection and fallbacks
if(CMAKE_VERSION VERSION_LESS "3.1")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # Check GCC version for C++ standard support
        execute_process(COMMAND ${CMAKE_CXX_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
        if(GCC_VERSION VERSION_GREATER "7.0")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
        elseif(GCC_VERSION VERSION_GREATER "4.9")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
        else()
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
        endif()
    else()
        # Default to C++11 for other compilers
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    endif()
else()
    # Modern CMake
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()

# Compiler and linker flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O3 -pthread")

# Linux-only linker optimizations
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--as-needed")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--as-needed")
endif()

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
    comparison.cpp
    visualization.cpp
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
    comparison.h
    visualization.h
    utils.h
)

# Create executable
add_executable(perf_test ${SOURCES})

# Direct library linking - bypasses ALL CMake pthread detection issues
# Order matters for static linking: dependencies first, dependents last

# Math library
target_link_libraries(perf_test m)

# RT library for Linux (POSIX shared memory/IPC)
if(UNIX AND NOT APPLE)
    # Try to find rt library, fallback to direct linking
    find_library(RT_LIB rt PATHS /usr/lib /usr/lib64 /lib /lib64 /usr/lib/x86_64-linux-gnu)
    if(RT_LIB)
        target_link_libraries(perf_test ${RT_LIB})
    else()
        target_link_libraries(perf_test rt)
    endif()
endif()

# Pthread library - COMPLETELY bypass CMake's find_package(Threads)
if(UNIX)
    # Try to find pthread library directly
    find_library(PTHREAD_LIB pthread PATHS 
        /usr/lib 
        /usr/lib64 
        /lib 
        /lib64 
        /usr/lib/x86_64-linux-gnu
        /usr/lib/i386-linux-gnu
        /usr/lib/arm-linux-gnueabihf
        /usr/lib/aarch64-linux-gnu
    )
    
    if(PTHREAD_LIB)
        message(STATUS "Found pthread library: ${PTHREAD_LIB}")
        target_link_libraries(perf_test ${PTHREAD_LIB})
    else()
        message(STATUS "Using direct pthread linking")
        target_link_libraries(perf_test pthread)
    endif()
endif()

# DL library for Linux (dynamic linking)
if(UNIX AND NOT APPLE)
    find_library(DL_LIB dl PATHS /usr/lib /usr/lib64 /lib /lib64 /usr/lib/x86_64-linux-gnu)
    if(DL_LIB)
        target_link_libraries(perf_test ${DL_LIB})
    else()
        target_link_libraries(perf_test dl)
    endif()
endif()

# Display final configuration
message(STATUS "==== Build Configuration ====")
message(STATUS "CMake version: ${CMAKE_VERSION}")
message(STATUS "System: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "CXX Flags: ${CMAKE_CXX_FLAGS}")
message(STATUS "Linker Flags: ${CMAKE_EXE_LINKER_FLAGS}")
message(STATUS "===============================")