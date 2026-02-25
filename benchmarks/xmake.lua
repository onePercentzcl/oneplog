-- ==============================================================================
-- onePlog Benchmarks - Standalone Build
-- onePlog 性能测试 - 独立构建
-- ==============================================================================

set_project("oneplog_benchmarks")
set_version("0.2.0")
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
add_includedirs(".")  -- For benchmark_utils.hpp

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
-- Optional: spdlog for comparison benchmarks
-- 可选：spdlog 用于对比测试
-- ==============================================================================
add_requires("spdlog", {system = false, configs = {header_only = true, fmt_external = false}, optional = true})

-- ==============================================================================
-- Helper function to create benchmark target
-- 创建基准测试目标的辅助函数
-- ==============================================================================
local function benchmark_target(name, source_file)
    target(name)
        set_kind("binary")
        add_files(source_file)
        add_files(oneplog_sources)
        add_files(fmt_sources)
        add_platform_libs()
        if has_package("spdlog") then
            add_packages("spdlog")
            add_defines("HAS_SPDLOG")
        end
    target_end()
end

-- ==============================================================================
-- Benchmark Targets / 基准测试目标
-- ==============================================================================

-- Sync mode benchmark / 同步模式性能测试
benchmark_target("benchmark_sync", "benchmark_sync.cpp")

-- Async mode benchmark / 异步模式性能测试
benchmark_target("benchmark_async", "benchmark_async.cpp")

-- Multi-process mode benchmark / 多进程模式性能测试
benchmark_target("benchmark_mproc", "benchmark_mproc.cpp")
