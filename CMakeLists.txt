cmake_minimum_required(VERSION 3.16)
project(oneplog VERSION 1.0.0 LANGUAGES CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(ONEPLOG_BUILD_TESTS "Build tests" OFF)
option(ONEPLOG_BUILD_EXAMPLES "Build examples" OFF)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Header-only library
add_library(oneplog INTERFACE)
target_include_directories(oneplog INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Installation
install(DIRECTORY include/oneplog DESTINATION include)
install(TARGETS oneplog EXPORT oneplogTargets)
install(EXPORT oneplogTargets
    FILE oneplogTargets.cmake
    NAMESPACE oneplog::
    DESTINATION lib/cmake/oneplog
)
