-- ==============================================================================
-- onePlog - High Performance Multi-Process Logging System
-- onePlog - 高性能多进程日志系统
-- ==============================================================================

set_project("oneplog")
set_version("0.1.3")
set_xmakever("2.7.0")

-- ==============================================================================
-- Options / 构建选项
-- ==============================================================================
option("shared", {default = false, description = "Build shared library / 构建动态库"})
option("headeronly", {default = false, description = "Header-only mode / 仅头文件模式"})
option("tests", {default = false, description = "Build tests / 构建测试"})
option("examples", {default = false, description = "Build examples / 构建示例"})

-- ==============================================================================
-- C++ Standard / C++ 标准
-- ==============================================================================
set_languages("c++17")

-- ==============================================================================
-- Dependencies / 依赖
-- ==============================================================================
-- fmt library is included locally in include/fmt and src/
-- fmt 库已本地包含在 include/fmt 和 src/ 目录下

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
-- Source Files / 源文件
-- ==============================================================================
local oneplog_sources = {
    "src/oneplog/instantiations.cpp"
}

-- fmt library sources (bundled with oneplog)
-- fmt 库源文件（内置于 oneplog）
local fmt_sources = {
    "src/fmt/format.cc",
    "src/fmt/os.cc"
}

-- ==============================================================================
-- Library Target / 库目标
-- ==============================================================================
if has_config("headeronly") then
    -- Header-only library / 仅头文件库
    target("oneplog")
        set_kind("headeronly")
        add_headerfiles("include/(oneplog/*.hpp)")
        add_includedirs("include", {public = true})
        add_defines("ONEPLOG_HEADER_ONLY", {public = true})
    target_end()
else
    target("oneplog")
        if has_config("shared") then
            set_kind("shared")
        else
            set_kind("static")
        end
        
        add_files(oneplog_sources)
        add_files(fmt_sources)
        add_headerfiles("include/(oneplog/*.hpp)")
        add_headerfiles("include/(fmt/*.h)")
        add_includedirs("include", {public = true})
        
        -- Platform-specific libraries / 平台特定库
        if is_plat("linux") then
            add_syslinks("pthread", "rt")
        elseif is_plat("macosx") then
            add_syslinks("pthread")
        end
    target_end()
end

-- ==============================================================================
-- spdlog for comparison benchmark / spdlog 用于对比测试
-- Force using xmake repo version with bundled fmt
-- ==============================================================================
add_requires("spdlog", {system = false, configs = {header_only = true, fmt_external = false}, optional = true})

-- ==============================================================================
-- Tests / 测试
-- ==============================================================================
if has_config("tests") then
    add_requires("gtest")
    -- RapidCheck is optional for property-based testing
    -- RapidCheck 是可选的，用于属性测试
    add_requires("rapidcheck", {optional = true})
    
    target("oneplog_tests")
        set_kind("binary")
        set_default(false)
        add_files("tests/*.cpp")
        add_deps("oneplog")
        add_packages("gtest")
        add_links("gtest_main")
        if has_package("rapidcheck") then
            add_packages("rapidcheck")
            add_defines("ONEPLOG_HAS_RAPIDCHECK")
        end
        add_includedirs("tests")
    target_end()
end

-- ==============================================================================
-- Examples / 示例
-- ==============================================================================
if has_config("examples") then
    -- Sync mode example / 同步模式示例
    target("sync_example")
        set_kind("binary")
        set_default(false)
        add_files("example/sync_example.cpp")
        add_deps("oneplog")
    target_end()
    
    -- Async mode example / 异步模式示例
    target("async_example")
        set_kind("binary")
        set_default(false)
        add_files("example/async_example.cpp")
        add_deps("oneplog")
    target_end()
    
    -- Multi-process mode example / 多进程模式示例
    target("mproc_example")
        set_kind("binary")
        set_default(false)
        add_files("example/mproc_example.cpp")
        add_deps("oneplog")
    target_end()
    
    -- WFC example / WFC 示例
    target("wfc_example")
        set_kind("binary")
        set_default(false)
        add_files("example/wfc_example.cpp")
        add_deps("oneplog")
    target_end()
    
    -- Exec child process example / Exec 子进程示例
    target("exec_example")
        set_kind("binary")
        set_default(false)
        add_files("example/exec_example.cpp")
        add_deps("oneplog")
    target_end()
    
    -- Multi-process multi-thread example / 多进程多线程示例
    target("multithread_example")
        set_kind("binary")
        set_default(false)
        add_files("example/multithread_example.cpp")
        add_deps("oneplog")
    target_end()
    
    -- Performance benchmark / 性能基准测试
    target("benchmark")
        set_kind("binary")
        set_default(false)
        add_files("example/benchmark.cpp")
        add_deps("oneplog")
    target_end()
    
    -- WFC overhead benchmark / WFC 开销基准测试
    target("benchmark_wfc_overhead")
        set_kind("binary")
        set_default(false)
        add_files("example/benchmark_wfc_overhead.cpp")
        add_deps("oneplog")
    target_end()
    
    -- Name lookup benchmark / 名称查找基准测试
    target("benchmark_name_lookup")
        set_kind("binary")
        set_default(false)
        add_files("example/benchmark_name_lookup.cpp")
        add_deps("oneplog")
    target_end()
    
    -- spdlog comparison benchmark / spdlog 对比测试
    -- This target uses header-only mode to avoid fmt conflicts with spdlog
    -- 此目标使用仅头文件模式以避免与 spdlog 的 fmt 冲突
    target("benchmark_compare")
        set_kind("binary")
        set_default(false)
        add_files("example/benchmark_compare.cpp")
        add_includedirs("include")
        -- Use header-only mode / 使用仅头文件模式
        add_defines("ONEPLOG_HEADER_ONLY")
        -- Add fmt sources (still needed for fmt formatting)
        -- 添加 fmt 源文件（仍需要用于 fmt 格式化）
        add_files("src/fmt/format.cc")
        add_files("src/fmt/os.cc")
        if is_plat("linux") then
            add_syslinks("pthread", "rt")
        elseif is_plat("macosx") then
            add_syslinks("pthread")
        end
        if has_package("spdlog") then
            add_packages("spdlog")
            add_defines("HAS_SPDLOG")
        end
    target_end()
end
