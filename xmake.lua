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
