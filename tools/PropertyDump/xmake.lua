set_xmakever("3.0.0")

includes("../..")

set_project("PropertyDump")
set_version("1.0.0")
set_languages("c++23")
set_warnings("allextra")

add_rules("mode.debug", "mode.releasedbg")

target("PropertyDump")
    add_rules("commonlibre5.plugin", { name = "PropertyDump", author = "LuBuCake" })
    set_symbols("debug")
    add_files("src/**.cpp")
