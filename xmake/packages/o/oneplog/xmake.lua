package("oneplog")
    set_homepage("https://github.com/onePercentzcl/oneplog")
    set_description("High performance C++17 multi-process logging system")
    set_license("MIT")
    
    add_urls("https://github.com/onePercentzcl/oneplog.git")
    add_versions("latest", "main")
    
    add_configs("header_only", {description = "Use header-only mode", default = false, type = "boolean"})
    
    on_install(function (package)
        os.cp("include", package:installdir())
        if not package:config("header_only") then
            os.cp("src", package:installdir())
        end
    end)
    
    on_test(function (package)
        assert(package:has_cxxincludes("oneplog/oneplog.hpp"))
    end)
package_end()
