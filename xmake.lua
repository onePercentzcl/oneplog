-- ==============================================================================
-- onePlog - High Performance Multi-Process Logging System
-- onePlog - 高性能多进程日志系统
-- ==============================================================================

set_project("oneplog")
set_version("0.1.0")
set_xmakever("2.7.0")

-- ==============================================================================
-- Options / 构建选项
-- ==============================================================================
option("shared", {default = false, description = "Build shared library / 构建动态库"})
option("headeronly", {default = false, description = "Header-only mode / 仅头文件模式"})
option("tests", {default = true, description = "Build tests / 构建测试"})
option("examples", {default = true, description = "Build examples / 构建示例"})
option("use_fmt", {default = true, description = "Use fmt library / 使用 fmt 库"})

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
    "src/log_entry.cpp",
    "src/heap_ring_buffer.cpp",
    "src/shared_ring_buffer.cpp",
    "src/format.cpp",
    "src/sink.cpp",
    "src/pipeline_thread.cpp",
    "src/writer_thread.cpp",
    "src/logger.cpp",
    "src/memory_pool.cpp"
}

-- fmt library sources (when use_fmt is enabled)
-- fmt 库源文件（启用 use_fmt 时）
local fmt_sources = {
    "src/format.cc",
    "src/os.cc"
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
        add_headerfiles("include/(oneplog/*.hpp)")
        add_includedirs("include", {public = true})
        
        -- Platform-specific libraries / 平台特定库
        if is_plat("linux") then
            add_syslinks("pthread", "rt")
        elseif is_plat("macosx") then
            add_syslinks("pthread")
        end
        
        -- Optional: fmt library (local) / 可选: fmt 库（本地）
        if has_config("use_fmt") then
            add_files(fmt_sources)
            add_headerfiles("include/(fmt/*.h)")
            add_defines("ONEPLOG_USE_FMT", {public = true})
        end
    target_end()
end

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
    
    -- Performance benchmark / 性能基准测试
    target("benchmark")
        set_kind("binary")
        set_default(false)
        add_files("example/benchmark.cpp")
        add_deps("oneplog")
    target_end()
end
