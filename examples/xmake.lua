-- ==============================================================================
-- onePlog Examples - Standalone Build
-- onePlog 示例 - 独立构建
-- ==============================================================================

set_project("oneplog_examples")
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
        add_platform_libs()
    target_end()
end

-- ==============================================================================
-- Example Targets / 示例目标
-- ==============================================================================

-- Sync mode example / 同步模式示例
example_target("example_sync", "example_sync.cpp")

-- Async mode example / 异步模式示例
example_target("example_async", "example_async.cpp")

-- Multi-process mode examples / 多进程模式示例
-- API demonstration (single process) / API 演示（单进程）
example_target("example_mproc", "example_mproc.cpp")

-- Fork-based multi-process (POSIX only) / 基于 fork 的多进程（仅 POSIX）
example_target("example_mproc_fork", "example_mproc_fork.cpp")

-- Standalone multi-process: owner (run first) / 独立多进程：所有者（先运行）
example_target("example_mproc_owner", "example_mproc_owner.cpp")

-- Standalone multi-process: worker (run after owner) / 独立多进程：工作者（在所有者之后运行）
example_target("example_mproc_worker", "example_mproc_worker.cpp")
