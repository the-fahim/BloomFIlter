cmake_minimum_required(VERSION 3.25)

# ============================================================================
#  BloomFilter — Thread-Safe, XXH3-powered Bloom Filter in C++23
#  Compiler: Clang   Build System: Ninja
# ============================================================================

project(BloomFilter
    VERSION     1.0.0
    DESCRIPTION "High-performance thread-safe Bloom filter with XXH3 hashing"
    LANGUAGES   CXX
)

# ---------------------------------------------------------------------------
#  Force clang / clang++ (must be set BEFORE project() in general; kept here
#  as a fallback in case the user drives cmake directly without -DCMAKE_*).
# ---------------------------------------------------------------------------
if(NOT DEFINED CMAKE_CXX_COMPILER)
    find_program(CLANGXX clang++ REQUIRED)
    set(CMAKE_CXX_COMPILER "${CLANGXX}" CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------------------
#  C++23 standard
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD          23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

# ---------------------------------------------------------------------------
#  Default build type
# ---------------------------------------------------------------------------
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ---------------------------------------------------------------------------
#  Compiler flags
# ---------------------------------------------------------------------------
add_compile_options(
    -Wall -Wextra -Wpedantic
    -Wno-unused-parameter

    # Performance
    $<$<CONFIG:Release>:-O3>
    $<$<CONFIG:Release>:-march=native>
    $<$<CONFIG:Release>:-ffast-math>
    $<$<CONFIG:Debug>:-O0>
    $<$<CONFIG:Debug>:-g3>
    $<$<CONFIG:Debug>:-fsanitize=address,thread>
)

# Sanitizer linker flags for Debug
add_link_options(
    $<$<CONFIG:Debug>:-fsanitize=address,thread>
)

# ---------------------------------------------------------------------------
#  Find Boost (header-only parts we use: hash2)
# ---------------------------------------------------------------------------
find_package(Boost 1.70 REQUIRED)

# ---------------------------------------------------------------------------
#  Find system xxhash
# ---------------------------------------------------------------------------
find_library(XXHASH_LIB xxhash REQUIRED)
find_path(XXHASH_INCLUDE_DIR xxhash.h REQUIRED)

message(STATUS "xxhash lib     : ${XXHASH_LIB}")
message(STATUS "xxhash include : ${XXHASH_INCLUDE_DIR}")

# ---------------------------------------------------------------------------
#  Library: BloomFilterLib
# ---------------------------------------------------------------------------
add_library(BloomFilterLib STATIC
    src/bloom_filter.cpp
    src/hash_function.cpp
    src/threading.cpp
)

target_include_directories(BloomFilterLib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${XXHASH_INCLUDE_DIR}
        ${Boost_INCLUDE_DIRS}
)

target_link_libraries(BloomFilterLib
    PUBLIC
        ${XXHASH_LIB}
)

# Thread support
find_package(Threads REQUIRED)
target_link_libraries(BloomFilterLib PUBLIC Threads::Threads)

# ---------------------------------------------------------------------------
#  Executable: bloom_filter
# ---------------------------------------------------------------------------
add_executable(bloom_filter src/main.cpp)

target_link_libraries(bloom_filter PRIVATE BloomFilterLib)

# ---------------------------------------------------------------------------
#  Compilation database (for clangd / IDE tooling)
# ---------------------------------------------------------------------------
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
#  Print summary
# ---------------------------------------------------------------------------
message(STATUS "")
message(STATUS "=== BloomFilter Build Configuration ===")
message(STATUS "  Compiler   : ${CMAKE_CXX_COMPILER}")
message(STATUS "  Standard   : C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Build type : ${CMAKE_BUILD_TYPE}")
message(STATUS "  Generator  : ${CMAKE_GENERATOR}")
message(STATUS "========================================")
message(STATUS "")
