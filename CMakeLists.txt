cmake_minimum_required(VERSION 3.16)
project(oneplog VERSION 1.0.0 LANGUAGES CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(ONEPLOG_BUILD_TESTS "Build tests" OFF)
option(ONEPLOG_BUILD_EXAMPLES "Build examples" OFF)
option(ONEPLOG_HEADER_ONLY "Build as header-only library" ON)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Library target
if(ONEPLOG_HEADER_ONLY)
    # Header-only library
    add_library(oneplog INTERFACE)
    target_include_directories(oneplog INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
else()
    # Compiled library (currently empty, will add sources as needed)
    # 编译库（当前为空，将根据需要添加源文件）
    add_library(oneplog INTERFACE)
    target_include_directories(oneplog INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
    message(WARNING "Non-header-only mode is not yet implemented. Using header-only mode.")
endif()

# Tests
if(ONEPLOG_BUILD_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)
    include(GoogleTest)

    # Add RapidCheck
    set(RC_ENABLE_GTEST ON CACHE BOOL "Enable RapidCheck GTest integration")
    add_subdirectory(external/rapidcheck EXCLUDE_FROM_ALL)

    # BinarySnapshot tests (header-only, no .cpp needed)
    add_executable(binary_snapshot_test tests/binary_snapshot_test.cpp)
    target_link_libraries(binary_snapshot_test PRIVATE GTest::gtest GTest::gtest_main)
    target_include_directories(binary_snapshot_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
    gtest_discover_tests(binary_snapshot_test)

    # BinarySnapshot property tests (using RapidCheck)
    add_executable(binary_snapshot_property_test tests/binary_snapshot_property_test.cpp)
    target_link_libraries(binary_snapshot_property_test PRIVATE GTest::gtest GTest::gtest_main rapidcheck)
    target_include_directories(binary_snapshot_property_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
    gtest_discover_tests(binary_snapshot_property_test)

    # BinarySnapshot RapidCheck tests
    add_executable(binary_snapshot_rc_test tests/binary_snapshot_rc_test.cpp)
    target_link_libraries(binary_snapshot_rc_test PRIVATE GTest::gtest GTest::gtest_main rapidcheck)
    target_include_directories(binary_snapshot_rc_test PRIVATE 
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/external/rapidcheck/include
        ${CMAKE_SOURCE_DIR}/external/rapidcheck/extras/gtest/include
    )
    gtest_discover_tests(binary_snapshot_rc_test)

    # LogEntry tests
    add_executable(log_entry_test tests/log_entry_test.cpp)
    target_link_libraries(log_entry_test PRIVATE GTest::gtest GTest::gtest_main)
    target_include_directories(log_entry_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
    gtest_discover_tests(log_entry_test)

    # LogEntry layout test (standalone executable for manual verification)
    add_executable(log_entry_layout_test tests/log_entry_layout_test.cpp)
    target_include_directories(log_entry_layout_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
endif()

# Installation
install(DIRECTORY include/oneplog DESTINATION include)
install(TARGETS oneplog EXPORT oneplogTargets)
install(EXPORT oneplogTargets
    FILE oneplogTargets.cmake
    NAMESPACE oneplog::
    DESTINATION lib/cmake/oneplog
)
