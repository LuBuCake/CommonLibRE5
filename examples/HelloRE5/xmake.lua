set_xmakever("3.0.0")

includes("../..")

set_project("HelloRE5")
set_version("1.0.0")
set_languages("c++23")
set_warnings("allextra")

add_rules("mode.debug", "mode.releasedbg")

target("HelloRE5")
    add_rules("commonlibre5.plugin", {
        name = "HelloRE5",
        author = "LuBuCake",
    })
    set_symbols("debug")
    add_files("src/**.cpp")
