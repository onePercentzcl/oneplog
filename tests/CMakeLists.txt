# ==============================================================================
# onePlog Tests - Standalone Build
# onePlog 测试 - 独立构建
# ==============================================================================
cmake_minimum_required(VERSION 3.14)

# Allow standalone build or as subdirectory
# 允许独立构建或作为子目录
if(NOT TARGET oneplog)
    project(oneplog_tests
        VERSION 0.2.0
        DESCRIPTION "onePlog Tests"
        LANGUAGES CXX
    )
    
    # Find installed oneplog or use parent directory
    # 查找已安装的 oneplog 或使用父目录
    find_package(oneplog QUIET)
    if(NOT oneplog_FOUND)
        # Use parent directory for header-only mode
        # 使用父目录的仅头文件模式
        add_library(oneplog INTERFACE)
        target_include_directories(oneplog INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
        target_compile_definitions(oneplog INTERFACE ONEPLOG_HEADER_ONLY)
        
        # Platform-specific libraries / 平台特定库
        if(UNIX AND NOT APPLE)
            target_link_libraries(oneplog INTERFACE pthread rt)
        elseif(UNIX AND APPLE)
            target_link_libraries(oneplog INTERFACE pthread)
        endif()
    endif()
    
    # Set source directory for external dependencies
    # 设置外部依赖的源目录
    set(ONEPLOG_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
else()
    set(ONEPLOG_SOURCE_DIR ${CMAKE_SOURCE_DIR})
endif()

# ==============================================================================
# C++ Standard / C++ 标准
# ==============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ==============================================================================
# Compiler Flags / 编译器标志
# ==============================================================================
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g -O0)
    else()
        add_compile_options(-O3)
    endif()
elseif(MSVC)
    add_compile_options(/W4)
endif()

# ==============================================================================
# Dependencies / 依赖 (使用本地目录)
# ==============================================================================

# Google Test - 本地路径
add_subdirectory(${ONEPLOG_SOURCE_DIR}/external/googletest ${CMAKE_BINARY_DIR}/googletest)

# RapidCheck - 本地路径
set(RC_ENABLE_GTEST ON CACHE BOOL "" FORCE)
set(RC_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(${ONEPLOG_SOURCE_DIR}/external/rapidcheck ${CMAKE_BINARY_DIR}/rapidcheck)

# ==============================================================================
# Test Sources / 测试源文件
# ==============================================================================
set(TEST_SOURCES
    test_level.cpp
    test_binary_snapshot.cpp
    test_log_entry.cpp
    test_heap_ring_buffer.cpp
    test_shared_ring_buffer.cpp
    test_shared_memory.cpp
    test_name_manager.cpp
    test_direct_mapping_table.cpp
    test_array_mapping_table.cpp
    test_optimized_name_manager.cpp
    test_logger_config.cpp
    test_logger.cpp
)

# ==============================================================================
# Test Executable / 测试可执行文件
# ==============================================================================
add_executable(oneplog_tests ${TEST_SOURCES})

target_include_directories(oneplog_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ONEPLOG_SOURCE_DIR}/external/rapidcheck/extras/gtest/include
)

# Add bundled fmt sources / 添加内置 fmt 源码
target_sources(oneplog_tests PRIVATE
    ${ONEPLOG_SOURCE_DIR}/src/fmt/format.cc
    ${ONEPLOG_SOURCE_DIR}/src/fmt/os.cc
)

# Enable RapidCheck property-based testing / 启用 RapidCheck 属性测试
target_compile_definitions(oneplog_tests PRIVATE ONEPLOG_HAS_RAPIDCHECK)

# Link libraries - order matters!
target_link_libraries(oneplog_tests PRIVATE
    oneplog
    rapidcheck_gtest
    rapidcheck
    gtest_main
    gtest
)

# ==============================================================================
# Register Tests / 注册测试
# ==============================================================================
enable_testing()
include(GoogleTest)
gtest_discover_tests(oneplog_tests)
