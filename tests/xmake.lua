-- ==============================================================================
-- onePlog Tests - Standalone Build
-- onePlog 测试 - 独立构建
-- ==============================================================================

set_project("oneplog_tests")
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
-- Dependencies / 依赖
-- ==============================================================================
add_requires("gtest")
-- RapidCheck is optional for property-based testing
-- RapidCheck 是可选的，用于属性测试
add_requires("rapidcheck", {optional = true})

-- ==============================================================================
-- Source Files / 源文件
-- ==============================================================================
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
-- Test Target / 测试目标
-- ==============================================================================
target("oneplog_tests")
    set_kind("binary")
    
    -- Test source files / 测试源文件
    add_files("test_level.cpp")
    add_files("test_binary_snapshot.cpp")
    add_files("test_log_entry.cpp")
    add_files("test_heap_ring_buffer.cpp")
    add_files("test_shared_ring_buffer.cpp")
    add_files("test_shared_memory.cpp")
    add_files("test_name_manager.cpp")
    add_files("test_direct_mapping_table.cpp")
    add_files("test_array_mapping_table.cpp")
    add_files("test_optimized_name_manager.cpp")
    add_files("test_logger_config.cpp")
    add_files("test_logger.cpp")
    
    -- Add fmt sources / 添加 fmt 源文件
    add_files(fmt_sources)
    
    -- Dependencies / 依赖
    add_packages("gtest")
    add_links("gtest_main")
    
    -- RapidCheck support / RapidCheck 支持
    if has_package("rapidcheck") then
        add_packages("rapidcheck")
        add_defines("ONEPLOG_HAS_RAPIDCHECK")
    end
    
    -- Platform libraries / 平台库
    add_platform_libs()
    
    -- Include paths / 包含路径
    add_includedirs(".")
target_end()
