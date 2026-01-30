package("oneplog")
    set_homepage("https://github.com/onePercentzcl/oneplog")
    set_description("High performance C++17 multi-process logging system / 高性能 C++17 多进程日志系统")
    set_license("MIT")
    
    add_urls("https://github.com/onePercentzcl/oneplog/archive/refs/tags/$(version).tar.gz",
             "https://github.com/onePercentzcl/oneplog.git")
    add_versions("v0.1.3", "")  -- SHA256 需要在发布后获取
    add_versions("v0.1.0", "")  -- SHA256 需要在发布后获取
    add_versions("v0.0.1", "")  -- SHA256 需要在发布后获取
    
    add_configs("shared", {description = "Build shared library", default = false, type = "boolean"})
    add_configs("header_only", {description = "Use header-only mode", default = false, type = "boolean"})
    add_configs("use_fmt", {description = "Use bundled fmt library", default = true, type = "boolean"})
    
    if is_plat("linux") then
        add_syslinks("pthread", "rt")
    elseif is_plat("macosx") then
        add_syslinks("pthread")
    end
    
    on_install(function (package)
        local configs = {}
        if package:config("shared") then
            configs.kind = "shared"
        elseif package:config("header_only") then
            configs.kind = "headeronly"
        end
        
        import("package.tools.xmake").install(package, configs)
    end)
    
    on_test(function (package)
        assert(package:check_cxxsnippets({test = [[
            #include <oneplog/oneplog.hpp>
            void test() {
                oneplog::Init();
                log::Info("test");
                oneplog::Shutdown();
            }
        ]]}, {configs = {languages = "c++17"}}))
    end)
package_end()
