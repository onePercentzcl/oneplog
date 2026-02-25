# ==============================================================================
# onePlog Examples - Standalone Build
# onePlog 示例 - 独立构建
# ==============================================================================
cmake_minimum_required(VERSION 3.14)

# Allow standalone build or as subdirectory
# 允许独立构建或作为子目录
if(NOT TARGET oneplog)
    project(oneplog_examples
        VERSION 0.2.0
        DESCRIPTION "onePlog Examples"
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
# Helper function to create example target
# 创建示例目标的辅助函数
# ==============================================================================
function(add_oneplog_example name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE oneplog)
    if(TARGET oneplog_fmt)
        target_link_libraries(${name} PRIVATE oneplog_fmt)
    endif()
endfunction()

# ==============================================================================
# Example Targets / 示例目标
# ==============================================================================

# Sync mode example / 同步模式示例
add_oneplog_example(example_sync example_sync.cpp)

# Async mode example / 异步模式示例
add_oneplog_example(example_async example_async.cpp)

# Multi-process mode example / 多进程模式示例
add_oneplog_example(example_mproc example_mproc.cpp)
