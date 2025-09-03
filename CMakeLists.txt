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

# Add linker flags for better compatibility (Linux only)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # GCC-specific flags for Linux only
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--as-needed")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Clang-specific flags for Linux only
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--as-needed")
endif()

# Find required packages
find_package(Threads)

# Debug output for CMake 2.8.12.2 pthread issues
message(STATUS "CMake version: ${CMAKE_VERSION}")
message(STATUS "CMAKE_THREAD_LIBS_INIT: '${CMAKE_THREAD_LIBS_INIT}'")
message(STATUS "CMAKE_USE_PTHREADS_INIT: ${CMAKE_USE_PTHREADS_INIT}")
message(STATUS "CMAKE_HAVE_THREADS_LIBRARY: ${CMAKE_HAVE_THREADS_LIBRARY}")

# Handle pthread linking for older systems
if(CMAKE_USE_PTHREADS_INIT)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
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

# Link libraries in correct order (important for static linking)
# Math library first
target_link_libraries(perf_test m)

# RT library for Linux (shared memory/IPC)
if(UNIX AND NOT APPLE)
    find_library(RT_LIB rt)
    if(RT_LIB)
        target_link_libraries(perf_test ${RT_LIB})
    else()
        target_link_libraries(perf_test rt)
    endif()
endif()

# Pthread library (CMake 2.8 compatible with multiple fallbacks)
if(CMAKE_THREAD_LIBS_INIT)
    message(STATUS "Original CMAKE_THREAD_LIBS_INIT: '${CMAKE_THREAD_LIBS_INIT}'")
    
    # Fix common CMake 2.8.x bugs with pthread library names
    set(FIXED_THREAD_LIBS "${CMAKE_THREAD_LIBS_INIT}")
    string(REPLACE "-lpthreads" "-lpthread" FIXED_THREAD_LIBS "${FIXED_THREAD_LIBS}")
    string(REPLACE "pthreads" "pthread" FIXED_THREAD_LIBS "${FIXED_THREAD_LIBS}")
    
    message(STATUS "Fixed pthread libs: '${FIXED_THREAD_LIBS}'")
    
    # Check if the fixed version is different and warn user
    if(NOT "${CMAKE_THREAD_LIBS_INIT}" STREQUAL "${FIXED_THREAD_LIBS}")
        message(STATUS "Applied CMake 2.8.x pthread library name fix")
    endif()
    
    target_link_libraries(perf_test ${FIXED_THREAD_LIBS})
else()
    message(STATUS "CMAKE_THREAD_LIBS_INIT is empty, using fallback methods")
    
    # Fallback approaches for systems where find_package(Threads) fails
    if(UNIX)
        # Try pthread library directly
        find_library(PTHREAD_LIB pthread)
        if(PTHREAD_LIB)
            message(STATUS "Found pthread library: ${PTHREAD_LIB}")
            target_link_libraries(perf_test ${PTHREAD_LIB})
        else()
            message(STATUS "Using direct pthread linking as last resort")
            # Last resort - link pthread by name
            target_link_libraries(perf_test pthread)
        endif()
    endif()
endif()

# Additional system libraries that might be needed on some Linux systems
if(UNIX AND NOT APPLE)
    # Some systems need dl for dynamic linking
    find_library(DL_LIB dl)
    if(DL_LIB)
        target_link_libraries(perf_test ${DL_LIB})
    endif()
endif()