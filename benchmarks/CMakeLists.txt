# ==============================================================================
# onePlog Benchmarks - Standalone Build
# onePlog 性能测试 - 独立构建
# ==============================================================================
cmake_minimum_required(VERSION 3.14)

# Allow standalone build or as subdirectory
# 允许独立构建或作为子目录
if(NOT TARGET oneplog)
    project(oneplog_benchmarks
        VERSION 0.2.0
        DESCRIPTION "onePlog Performance Benchmarks"
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
        
        # Add fmt sources
        # 添加 fmt 源文件
        add_library(oneplog_fmt STATIC
            ${CMAKE_CURRENT_SOURCE_DIR}/../src/fmt/format.cc
            ${CMAKE_CURRENT_SOURCE_DIR}/../src/fmt/os.cc
        )
        target_include_directories(oneplog_fmt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)
        
        # Platform-specific libraries / 平台特定库
        if(UNIX AND NOT APPLE)
            target_link_libraries(oneplog INTERFACE pthread rt)
        elseif(UNIX AND APPLE)
            target_link_libraries(oneplog INTERFACE pthread)
        endif()
    endif()
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
# Benchmark Utils Include Path / 基准测试工具包含路径
# ==============================================================================
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# ==============================================================================
# Optional: spdlog for comparison benchmarks
# 可选：spdlog 用于对比测试
# spdlog 位于 external/spdlog 目录
# ==============================================================================
set(SPDLOG_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../external/spdlog/include")
if(EXISTS "${SPDLOG_INCLUDE_DIR}/spdlog/spdlog.h")
    set(HAS_SPDLOG TRUE)
    add_compile_definitions(HAS_SPDLOG)
    # 注意：spdlog 会自动定义 SPDLOG_HEADER_ONLY，无需手动定义
    # Note: spdlog auto-defines SPDLOG_HEADER_ONLY, no need to define manually
    # 不使用 SPDLOG_FMT_EXTERNAL，让 spdlog 使用自己内置的 fmt
    # Don't use SPDLOG_FMT_EXTERNAL, let spdlog use its bundled fmt
    include_directories(${SPDLOG_INCLUDE_DIR})
    message(STATUS "Found spdlog at: ${SPDLOG_INCLUDE_DIR}")
else()
    message(WARNING "spdlog not found at: ${SPDLOG_INCLUDE_DIR}")
endif()

# ==============================================================================
# Helper function to create benchmark target
# 创建基准测试目标的辅助函数
# ==============================================================================
function(add_oneplog_benchmark name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE oneplog)
    if(TARGET oneplog_fmt)
        target_link_libraries(${name} PRIVATE oneplog_fmt)
    endif()
    # spdlog 使用 header-only 模式，无需链接
    # spdlog uses header-only mode, no linking required
endfunction()

# ==============================================================================
# Benchmark Targets / 基准测试目标
# ==============================================================================

# Sync mode benchmark / 同步模式性能测试
add_oneplog_benchmark(benchmark_sync benchmark_sync.cpp)

# Async mode benchmark / 异步模式性能测试
add_oneplog_benchmark(benchmark_async benchmark_async.cpp)

# Multi-process mode benchmark / 多进程模式性能测试
add_oneplog_benchmark(benchmark_mproc benchmark_mproc.cpp)
