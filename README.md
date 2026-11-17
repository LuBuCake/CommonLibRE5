CommonLibRE5
============

Static C++ library for **Resident Evil 5** plugins loaded by [MTFE (MT Framework Extender)](https://github.com/LuBuCake/MTFE), in the role CommonLibF4 plays for F4SE: game addresses, chain-keeping hooks, the MT Framework engine types, reverse-engineered game types and the MTFE API wrapped in C++.

Target: Resident Evil 5, Steam, Gold Edition, build of Jan 23 2023 (executable timestamp `0x63E9CE00`), x86.

## Features

* **REL**: module info, offsets as RVAs, `Relocation<T>`, safe writes, `write_call5` / `write_branch5` / `write_vfunc` that return the previous target (so every hook keeps the chain), pattern scans, `invoke_vfunc` / `invoke_thiscall`.
* **MT Framework engine**: `MtDTI` (the engine's runtime type registry: find a class by name, `NewInstance`, `IsKindOf`), `MtObject`, **MT properties** (`MtPropertyList`: every reflected field of any object, by name), `sMain` and `cSystem`, the 47 system singletons, `cUnit` / `sUnit` and its 32 move lines.
* Generated from the executable's type registry: `RE::DTI::<class>` (4,355 classes), `RE::VTABLE::<class>` and `RE::kClasses`.
* **Game types**: characters (`uPawn`, `uCharacter`, `uPlayer`, `uEnemy`: health, inventory, partner), the damage pipeline (`HitInfo`, a hook on the enemy damage call, `Damage::Apply`), game and save state (`sInfoMan`, character and costume choice, chapter, money, options), items and weapons (item and weapon tables, debug names, the 24-slot inventory, ammo), input (logical pad buttons, action layer) and game key codes (`RE::GameKey`).
* **MTFE API**: plugin init, per-plugin log, loader messages, main-thread tasks and per-frame callbacks, and the **settings page** in the game's Options > MODS (`MTFE/Menu.h`: toggles, lists, numbers, hotkeys, buttons, saved in an INI).

Every field of a game type is either read from the engine's own property reflection or checked in the disassembly or at runtime; the comments say where each one comes from.

## Pre-requisites

* [Visual Studio](https://visualstudio.microsoft.com/) with the *Desktop development with C++* workload (MSVC x86) and the Windows SDK 10.0.26100.
* [XMake](https://xmake.io) 3.0 or newer.
* [MTFE](https://github.com/LuBuCake/MTFE) installed in the game to run the plugins.

## Writing a Plugin

Add CommonLibRE5 to your project (for example as a git submodule in `lib/CommonLibRE5`) and use the `commonlibre5.plugin` rule, which also generates the plugin's version data (`MTFEPlugin_Version`):

```lua
set_xmakever("3.0.0")

includes("lib/CommonLibRE5")

set_project("MyPlugin")
set_version("1.0.0")
set_languages("c++23")

add_rules("mode.debug", "mode.releasedbg")

target("MyPlugin")
    add_rules("commonlibre5.plugin", { name = "MyPlugin", author = "Me" })
    add_files("src/**.cpp")
```

```cpp
#include <MTFE/MTFE.h>
#include <RE/RE5.h>

namespace
{
    MTFE::Menu::Control HealKey;

    void OnFrame()
    {
        static RE::GameKey::Watch Key;
        if (Key.Pressed(HealKey.Get()))
        {
            if (auto* Player = RE::sPlayer::GetSingleton()->GetPlayer(0))
            {
                Player->SetHp(Player->HpMax());
            }
        }
    }

    void OnMessage(MTFE::Message* Message)
    {
        if (static_cast<MTFE::MessageType>(Message->type) == MTFE::MessageType::kGameInit)
        {
            MTFE::AddFrameCallback(OnFrame);
        }
    }
}

MTFE_PLUGIN_LOAD(const MTFE::LoadInterface* Interface)
{
    MTFE::Init(Interface);
    HealKey = MTFE::Menu::AddHotkey("iHealKey", "Heal key", "Heals player 1.", RE::GameKey::kF8);
    MTFE::Log::Info("MyPlugin loaded");
    return MTFE::RegisterListener(OnMessage);
}
```

Options of the rule: `name`, `author`, `version` (otherwise `set_version`), `runtimes` (list of `MTFE_RUNTIME_*`, default Steam 2023), `address_independent = true` (pattern scans only, works on any runtime).

Build from a PowerShell or Developer prompt in the plugin folder:

```
xmake f -y -p windows -a x86 -m releasedbg --vs_sdkver=10.0.26100.0
xmake build
```

Copy the DLL into `<game>\MTFE\Plugins`. Logs go to `<game>\MTFE\Logs\<plugin name>.log`.

**At load time the game's C runtime has not run yet**: install hooks there, but use game types only after `PRE_WINMAIN` (engine types) or `GAME_INIT` (game systems). Hooks and callbacks can run on the game's loading threads too.

## Project Layout

```
include/MTFE/        MTFE API: PluginAPI.h (the C ABI shared with the loader), API.h, Log.h, Menu.h
include/REL/         relocations, hooks, pattern scans
include/RE/MT/       engine: MtDTI, MtObject, MtProperty, sMain, systems, cUnit / sUnit
include/RE/Game/     game: characters, damage, game info, items, input
include/RE/IDs_*.h   generated: DTI and vtable addresses of every engine class
src/                 implementations
examples/HelloRE5/   example plugin: every message, the systems, a match report and a settings page
tools/gen_ids.py     regenerates IDs_*.h from a class dump of the executable
tools/gen_items.py   regenerates ItemNames.h
tools/PropertyDump/  development plugin: dumps the MT properties of every class or of live objects
tools/DamageTrace/   development plugin: logs every health change with its callers
```

## License

[GPL-3.0](LICENSE) with the [modding exception](EXCEPTIONS).
