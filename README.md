CommonLibRE5
============

C++ library for writing **Resident Evil 5** plugins for [MTFE (MT Framework Extender)](https://github.com/LuBuCake/MTFE).

Supports Resident Evil 5 (Steam, Gold Edition, version 1.2.0), x86.

## Features

* **Hooks and addresses**: game addresses, memory writes, call and vtable hooks that keep other mods' hooks working, pattern scans.
* **Engine types**: the MT Framework's classes and runtime type info, object properties by name, the game's systems and units.
* **Game types**: characters (health, inventory, partner), damage, game and save state, items and weapons, input and key codes.
* **Every function of the game**, by class and name: `RE::ID::<class>::<function>` (address) and `RE::VFUNC::<class>::<function>` (vtable slot), listed in `data/Functions.tsv`.
* **Fields and properties of the game's classes**: `RE::FIELD::<class>::<field>` and `RE::PROP::<class>::<property>`.
* **MTFE API**: plugin startup, log, loader messages, main-thread tasks, per-frame callbacks and a settings page in the game's **Extras > Special Settings > MODS** (toggles, lists, numbers, hotkeys, buttons).

## Requirements

* [Visual Studio](https://visualstudio.microsoft.com/) with *Desktop development with C++* (MSVC x86) and the Windows SDK 10.0.26100.
* [XMake](https://xmake.io) 3.0 or newer.
* [MTFE](https://github.com/LuBuCake/MTFE) installed in the game to run the plugins.

## Writing a Plugin

Add CommonLibRE5 to your project (for example as a git submodule in `lib/CommonLibRE5`) and use the `commonlibre5.plugin` rule:

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
            if (auto* Player = RE::sPlayer::GetLocalPlayer())
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

Use the game's types only after the `GAME_INIT` message (`PRE_WINMAIN` for the engine types). Hooks can be installed at load.

Build from a PowerShell or Developer prompt in the plugin folder:

```
xmake f -y -p windows -a x86 -m releasedbg --vs_sdkver=10.0.26100.0
xmake build
```

Copy the DLL into `<game>\MTFE\Plugins`. The plugin's log is `<game>\MTFE\Logs\<plugin name>.log`.

`examples/HelloRE5` is a complete example plugin.

### Game Functions, Fields and Properties

```cpp
Player->SubHp(10);                                    // game methods of the types
auto& Pos = Player->Get(RE::FIELD::uPlayer::mPos);    // a field, by reference
Player->Get(RE::FIELD::uCharacter::Hp) = 1000;
auto Angle = Player->Get(RE::PROP::uPlayer::mAngle);  // a property, through the game's getter
Player->Set(RE::PROP::uPlayer::mAngle, Angle);        // ... and setter
```

A game function without a method is called by its id:

```cpp
using Func = bool(__stdcall*)(RE::cItem*);
static REL::Relocation<Func> IsInfiniteAmmo{ RE::ID::cItem::IsInfiniteAmmo };
```

## License

[GPL-3.0](LICENSE) with the [modding exception](EXCEPTIONS).
