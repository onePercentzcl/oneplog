-- ==============================================================================
-- onePlog Examples - Standalone Build
-- onePlog 示例 - 独立构建
-- ==============================================================================

set_project("oneplog_examples")
set_version("0.1.0")
set_xmakever("2.7.0")

-- ==============================================================================
-- C++ Standard / C++ 标准
-- ==============================================================================
set_languages("c++17")

-- ==============================================================================
-- Build Modes / 构建模式
-- ==============================================================================
add_rules("mode.debug", "mode.release")

if is_mode("debug") then
    set_symbols("debug")
    set_optimize("none")
    add_defines("DEBUG")
else
    set_symbols("hidden")
    set_optimize("fastest")
    add_defines("NDEBUG")
end

-- ==============================================================================
-- Compiler Flags / 编译器标志
-- ==============================================================================
if is_plat("linux", "macosx") then
    add_cxxflags("-Wall", "-Wextra", "-Wpedantic")
end

-- ==============================================================================
-- Include Paths / 包含路径
-- ==============================================================================
add_includedirs("../include")

-- ==============================================================================
-- Source Files / 源文件
-- ==============================================================================
local oneplog_sources = {
    "../src/oneplog/instantiations.cpp"
}

local fmt_sources = {
    "../src/fmt/format.cc",
    "../src/fmt/os.cc"
}

-- ==============================================================================
-- Platform Libraries / 平台库
-- ==============================================================================
local function add_platform_libs()
    if is_plat("linux") then
        add_syslinks("pthread", "rt")
    elseif is_plat("macosx") then
        add_syslinks("pthread")
    end
end

-- ==============================================================================
-- Helper function to create example target
-- 创建示例目标的辅助函数
-- ==============================================================================
local function example_target(name, source_file)
    target(name)
        set_kind("binary")
        add_files(source_file)
        add_files(oneplog_sources)
        add_files(fmt_sources)
        add_defines("ONEPLOG_USE_FMT")
        add_platform_libs()
    target_end()
end

-- ==============================================================================
-- Example Targets / 示例目标
-- ==============================================================================

-- Sync mode example / 同步模式示例
example_target("sync_example", "sync_example.cpp")

-- Async mode example / 异步模式示例
example_target("async_example", "async_example.cpp")

-- Multi-process mode example / 多进程模式示例
example_target("mproc_example", "mproc_example.cpp")

-- WFC example / WFC 示例
example_target("wfc_example", "wfc_example.cpp")

-- Exec child process example / Exec 子进程示例
example_target("exec_example", "exec_example.cpp")

-- Multi-process multi-thread example / 多进程多线程示例
example_target("multithread_example", "multithread_example.cpp")

-- Performance benchmark / 性能基准测试
example_target("benchmark", "benchmark.cpp")

-- WFC overhead benchmark / WFC 开销基准测试
example_target("benchmark_wfc_overhead", "benchmark_wfc_overhead.cpp")

-- Name lookup benchmark / 名称查找基准测试
example_target("benchmark_name_lookup", "benchmark_name_lookup.cpp")

-- DirectMappingTable benchmark / DirectMappingTable 基准测试
example_target("benchmark_direct_mapping", "benchmark_direct_mapping.cpp")

-- ArrayMappingTable benchmark / ArrayMappingTable 基准测试
example_target("benchmark_array_mapping", "benchmark_array_mapping.cpp")

-- OptimizedNameManager benchmark / OptimizedNameManager 基准测试
example_target("benchmark_optimized_name_manager", "benchmark_optimized_name_manager.cpp")

-- Shadow tail optimization benchmark / Shadow tail 优化基准测试
example_target("benchmark_shadow_tail", "benchmark_shadow_tail.cpp")

-- Stress test / 压力测试
example_target("stress_test", "stress_test.cpp")

-- Simple async test
target("test_async")
    set_kind("binary")
    add_files("test_async.cpp")
    add_files(oneplog_sources)
    add_files(fmt_sources)
    add_defines("ONEPLOG_USE_FMT")
    add_platform_libs()
target_end()

-- Simple async test - minimal
target("test_async_simple")
    set_kind("binary")
    add_files("test_async_simple.cpp")
    add_files(oneplog_sources)
    add_files(fmt_sources)
    add_defines("ONEPLOG_USE_FMT")
    add_platform_libs()
target_end()

-- Test async fastlogger pattern
target("test_async_fastlogger")
    set_kind("binary")
    add_files("test_async_fastlogger.cpp")
    add_files(oneplog_sources)
    add_files(fmt_sources)
    add_defines("ONEPLOG_USE_FMT")
    add_platform_libs()
target_end()

-- Test async exact copy
target("test_async_exact")
    set_kind("binary")
    add_files("test_async_exact.cpp")
    add_files(oneplog_sources)
    add_files(fmt_sources)
    add_defines("ONEPLOG_USE_FMT")
    add_platform_libs()
target_end()

-- Test real FastLogger
target("test_async_real")
    set_kind("binary")
    add_files("test_async_real.cpp")
    add_files(oneplog_sources)
    add_files(fmt_sources)
    add_defines("ONEPLOG_USE_FMT")
    add_platform_libs()
target_end()

-- Test variadic template
target("test_async_variadic")
    set_kind("binary")
    add_files("test_async_variadic.cpp")
    add_files(oneplog_sources)
    add_files(fmt_sources)
    add_defines("ONEPLOG_USE_FMT")
    add_platform_libs()
target_end()

-- Test with real MessageOnlyFormat
target("test_async_format")
    set_kind("binary")
    add_files("test_async_format.cpp")
    add_files(oneplog_sources)
    add_files(fmt_sources)
    add_defines("ONEPLOG_USE_FMT")
    add_platform_libs()
target_end()

-- Test template instantiation
target("test_async_template")
    set_kind("binary")
    add_files("test_async_template.cpp")
    add_files(oneplog_sources)
    add_files(fmt_sources)
    add_defines("ONEPLOG_USE_FMT")
    add_platform_libs()
target_end()

-- Test with oneplog::MessageOnlyFormat
target("test_async_oneplog_format")
    set_kind("binary")
    add_files("test_async_oneplog_format.cpp")
    add_files(oneplog_sources)
    add_files(fmt_sources)
    add_defines("ONEPLOG_USE_FMT")
    add_platform_libs()
target_end()

-- ==============================================================================
-- spdlog comparison benchmark (optional)
-- spdlog 对比测试（可选）
-- ==============================================================================

-- High contention comparison benchmark / 高竞争场景对比测试
target("benchmark_contention_compare")
    set_kind("binary")
    add_files("benchmark_contention_compare.cpp")
    add_files(oneplog_sources)
    add_files(fmt_sources)
    add_defines("ONEPLOG_USE_FMT")
    add_platform_libs()
    if has_package("spdlog") then
        add_packages("spdlog")
        add_defines("HAS_SPDLOG")
    end
target_end()

-- ==============================================================================
-- spdlog comparison benchmark (optional) - original
-- spdlog 对比测试（可选）
-- ==============================================================================
add_requires("spdlog", {system = false, configs = {header_only = true, fmt_external = false}, optional = true})

target("benchmark_compare")
    set_kind("binary")
    add_files("benchmark_compare.cpp")
    add_files(oneplog_sources)
    add_files(fmt_sources)
    add_defines("ONEPLOG_USE_FMT")
    add_platform_libs()
    if has_package("spdlog") then
        add_packages("spdlog")
        add_defines("HAS_SPDLOG")
    end
target_end()
