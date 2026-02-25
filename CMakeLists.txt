# ==============================================================================
# onePlog - High Performance Multi-Process Logging System
# onePlog - 高性能多进程日志系统
# ==============================================================================
cmake_minimum_required(VERSION 3.14)

project(oneplog
    VERSION 0.2.0
    DESCRIPTION "High performance C++17 multi-process logging system"
    LANGUAGES CXX
)

# ==============================================================================
# Options / 构建选项
# ==============================================================================
option(ONEPLOG_BUILD_SHARED "Build shared library / 构建动态库" OFF)
option(ONEPLOG_HEADER_ONLY "Header-only mode (no library) / 仅头文件模式" ON)
option(ONEPLOG_BUILD_TESTS "Build tests / 构建测试" OFF)
option(ONEPLOG_BUILD_EXAMPLES "Build examples / 构建示例" OFF)

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
# Header Files / 头文件
# ==============================================================================
set(ONEPLOG_HEADERS
    include/oneplog/common.hpp
    include/oneplog/logger.hpp
    include/oneplog/name_manager.hpp
    include/oneplog/oneplog.hpp
    include/oneplog/internal/array_mapping_table.hpp
    include/oneplog/internal/binary_snapshot.hpp
    include/oneplog/internal/direct_mapping_table.hpp
    include/oneplog/internal/fixed_name.hpp
    include/oneplog/internal/heap_memory.hpp
    include/oneplog/internal/heap_ring_buffer.hpp
    include/oneplog/internal/log_entry.hpp
    include/oneplog/internal/logger_config.hpp
    include/oneplog/internal/memory_pool.hpp
    include/oneplog/internal/optimized_name_manager.hpp
    include/oneplog/internal/pipeline_thread.hpp
    include/oneplog/internal/platform_lookup_table.hpp
    include/oneplog/internal/ring_buffer.hpp
    include/oneplog/internal/shared_memory.hpp
    include/oneplog/internal/shared_ring_buffer.hpp
    include/oneplog/internal/static_formats.hpp
    include/oneplog/sinks/sink.hpp
    include/oneplog/benchmark/latency_collector.hpp
    include/oneplog/benchmark/performance_benchmark.hpp
)

# ==============================================================================
# Library Target / 库目标
# ==============================================================================
if(ONEPLOG_HEADER_ONLY)
    # Header-only library / 仅头文件库
    add_library(oneplog INTERFACE)
    target_include_directories(oneplog INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
    target_compile_definitions(oneplog INTERFACE ONEPLOG_HEADER_ONLY)
    
    # Platform-specific libraries / 平台特定库
    if(UNIX AND NOT APPLE)
        target_link_libraries(oneplog INTERFACE pthread rt)
    elseif(UNIX AND APPLE)
        target_link_libraries(oneplog INTERFACE pthread)
    endif()
else()
    # Static or shared library with explicit template instantiations
    # 带显式模板实例化的静态或动态库
    set(ONEPLOG_SOURCES
        src/oneplog/instantiations.cpp
        src/fmt/format.cc
        src/fmt/os.cc
    )
    
    if(ONEPLOG_BUILD_SHARED)
        add_library(oneplog SHARED ${ONEPLOG_SOURCES})
        target_compile_definitions(oneplog 
            PRIVATE ONEPLOG_EXPORTS
            PUBLIC ONEPLOG_SHARED
        )
    else()
        add_library(oneplog STATIC ${ONEPLOG_SOURCES})
    endif()
    
    target_include_directories(oneplog PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
    
    # Platform-specific libraries / 平台特定库
    if(UNIX AND NOT APPLE)
        target_link_libraries(oneplog PUBLIC pthread rt)
    elseif(UNIX AND APPLE)
        target_link_libraries(oneplog PUBLIC pthread)
    endif()
endif()

# ==============================================================================
# Tests / 测试
# ==============================================================================
if(ONEPLOG_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ==============================================================================
# Examples / 示例
# ==============================================================================
if(ONEPLOG_BUILD_EXAMPLES)
    add_subdirectory(example)
endif()

# ==============================================================================
# Install / 安装
# ==============================================================================
include(GNUInstallDirs)

install(TARGETS oneplog
    EXPORT oneplog-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/oneplog
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT oneplog-targets
    FILE oneplog-config.cmake
    NAMESPACE oneplog::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/oneplog
)
