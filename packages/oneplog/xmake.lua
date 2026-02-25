package("oneplog")
    set_homepage("https://github.com/onePercentzcl/oneplog")
    set_description("High performance C++17 logging system")
    set_license("MIT")
    
    add_urls("https://github.com/onePercentzcl/oneplog/archive/refs/tags/$(version).tar.gz",
             "https://github.com/onePercentzcl/oneplog.git")
    add_versions("v0.2.1", "745c0f8a1f321368cde93c8e7667408d9b6a3de148daf524c51aca28908997b0")
    add_versions("v0.2.0", "b2d02191711d9919d9330d03c000c425aa0a1c05de831d029e55b2aa17d62f36")
    
    add_configs("shared", {description = "Build shared library", default = false, type = "boolean"})
    add_configs("header_only", {description = "Header-only mode", default = true, type = "boolean"})
    
    if is_plat("linux") then
        add_syslinks("pthread", "rt")
    elseif is_plat("macosx") then
        add_syslinks("pthread")
    end
    
    on_install(function (package)
        local configs = {"-DONEPLOG_BUILD_TESTS=OFF", "-DONEPLOG_BUILD_EXAMPLES=OFF"}
        
        if package:config("header_only") then
            table.insert(configs, "-DONEPLOG_HEADER_ONLY=ON")
        else
            table.insert(configs, "-DONEPLOG_HEADER_ONLY=OFF")
            if package:config("shared") then
                table.insert(configs, "-DONEPLOG_BUILD_SHARED=ON")
            end
        end
        
        import("package.tools.cmake").install(package, configs)
    end)
    
    on_test(function (package)
        assert(package:check_cxxsnippets({test = [[
            #include <oneplog/logger.hpp>
            void test() {
                oneplog::SyncLogger logger;
                logger.Info("test");
            }
        ]]}, {configs = {languages = "c++17"}}))
    end)
package_end()
