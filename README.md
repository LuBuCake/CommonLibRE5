CommonLibRE5
============

Static C++ library for **Resident Evil 5** plugins loaded by [MTFE (MT Framework Extender)](https://github.com/LuBuCake/MTFE), in the role CommonLibF4 plays for F4SE: game addresses, chain-keeping hooks, the MT Framework engine types, reverse-engineered game types and the MTFE API wrapped in C++.

Target: Resident Evil 5, Steam, Gold Edition, build of Jan 23 2023 (executable timestamp `0x63E9CE00`), x86.

## Features

* **REL**: module info, offsets as RVAs, `Relocation<T>`, safe writes, `write_call5` / `write_branch5` / `write_vfunc` that return the previous target (so every hook keeps the chain), pattern scans, `invoke_vfunc` / `invoke_thiscall`.
* **MT Framework engine**: `MtDTI` (the engine's runtime type registry: find a class by name, `NewInstance`, `IsKindOf`), `MtObject`, **MT properties** (`MtPropertyList`: every reflected field of any object, by name), `sMain` and `cSystem`, the 47 system singletons, `cUnit` / `sUnit` and its 32 move lines.
* Generated from the executable's type registry: `RE::DTI::<class>` (4,355 classes), `RE::VTABLE::<class>` and `RE::kClasses`.
* **Function database** (`data/Functions.tsv`): every function of the executable (37,196), with the class that owns it, its vtable slot and the bytes it pops (`ret N`). Named functions become `RE::ID::<class>::<function>` (addresses) and named vtable slots `RE::VFUNC::<class>::<function>` (slot numbers, inherited by derived classes). See [Function Database](#function-database).
* **Fields and properties**: `RE::FIELD::<class>::<field>` (offset and type: 6,427 from the engine's property reflection plus hand-mapped game state in `data/Fields.tsv`) and `RE::PROP::<class>::<property>` (properties read and written through the game's own getters/setters, on the object that registers them), inherited by derived classes: `Player->Get(RE::FIELD::uPlayer::mPos)`, `Object->Set(RE::PROP::<class>::<property>, Value)`.
* **Game types**: characters (`uPawn`, `uCharacter`, `uPlayer`, `uEnemy`: health, inventory, partner), the damage pipeline (`HitInfo`, a hook on the enemy damage call, `Damage::Apply`), game and save state (`sInfoMan`, character and costume choice, chapter, money, options), items and weapons (item and weapon tables, debug names, the 24-slot inventory, ammo), input (logical pad buttons, action layer) and game key codes (`RE::GameKey`).
* **MTFE API**: plugin init, per-plugin log, loader messages, main-thread tasks and per-frame callbacks, and the **settings page** in the game's Extras > Special Settings > MODS (`MTFE/Menu.h`: toggles, lists, numbers, hotkeys, buttons, saved in an INI).

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

Options of the rule: `name`, `author`, `version` (otherwise `set_version`), `runtimes` (list of `MTFE_RUNTIME_*`, default Steam 2023), `address_independent = true` (pattern scans only, works on any runtime).

Build from a PowerShell or Developer prompt in the plugin folder:

```
xmake f -y -p windows -a x86 -m releasedbg --vs_sdkver=10.0.26100.0
xmake build
```

Copy the DLL into `<game>\MTFE\Plugins`. Logs go to `<game>\MTFE\Logs\<plugin name>.log`.

**At load time the game's C runtime has not run yet**: install hooks there, but use game types only after `PRE_WINMAIN` (engine types) or `GAME_INIT` (game systems). Hooks and callbacks can run on the game's loading threads too.

## Function Database

`data/Functions.tsv` lists every function of `re5dx9.exe`:

| Column | |
| --- | --- |
| `rva`, `va` | address (`rva` = `va` - `0x400000`, what `REL::Offset` takes) |
| `class`, `name` | owner and name; empty when nobody has named it yet |
| `kind` | `virtual` (a vtable target), `property` (getter/setter of an MT property), `ctor`, `dtor`, `getDTI`, `library` (C runtime), `thunk`, `function` |
| `slot` | vtable slot of a virtual function |
| `purge` | bytes the function pops (`ret N`): 0 means `__cdecl` or no stack arguments |
| `origin` | where the name comes from: `names`, `slot`, `property`, `ghidra`, `auto` (`vfNNN`) |

Names are given in two hand-edited files; everything else is generated:

* `data/Names.tsv`: a function by address (`0x84C200  cItem  IsInfiniteAmmo  bool __stdcall(cItem* item)`).
* `data/VFuncs.tsv`: a vtable slot (`uPawn  69  SubHp  void(std::int16_t value)`). The name applies to the slot in the class and in every class derived from it, so each override gets it (`uPlayer::SubHp` from `uPawn`'s slot 69).

Classes that own a virtual function are found from the vtables (the class whose vtable has the function in a slot where its parent's vtable has something else); getters and setters from the engine's property reflection. Unnamed virtual functions are `vfNNN` (their slot) in the database and in Ghidra.

In C++, game methods call the game the way the CommonLibF4 does: virtual functions through their slot, so the object's own override runs, and the rest through their address:

```cpp
// RE/Game/Character.h
void SubHp(std::int16_t Value) { REL::InvokeVFunc<void>(this, VFUNC::uPawn::SubHp, Value); }

bool IsInfiniteAmmo()
{
    using Func = bool(__stdcall*)(cItem*);
    static REL::Relocation<Func> Call{ ID::cItem::IsInfiniteAmmo };
    return Call(this);
}
```

A method only goes into a class once its signature is checked: the calling convention follows the function's `ret N`, not Ghidra's guess, and `tools/gen_functions.py` warns when a signature in the data files does not match the bytes the function pops.

To regenerate (with Ghidra and the program `re5dx9_unpacked.exe`, a memory dump of the running game):

1. Run `tools/ghidra/RE5_ExportFunctions.java` with an output folder: `functions.tsv` and `vtables.tsv`. Add `properties.tsv` from `tools/PropertyDump`.
2. `python tools/gen_functions.py <output folder> <classes.csv>`: writes `data/Functions.tsv`, `include/RE/IDs_Functions.h` and `include/RE/IDs_VFuncs.h`. Names given by hand in Ghidra that the data files do not have yet are added to `data/Names.tsv`.
3. Run `tools/ghidra/RE5_ApplyNames.java` with `data/Functions.tsv`: creates the functions the analysis missed and writes the names back to Ghidra.

## Fields and Properties

```cpp
auto& Pos = Player->Get(RE::FIELD::uPlayer::mPos);         // a field: MtVector3& into the object (declared in uCoord)
Player->Get(RE::FIELD::uCharacter::Hp) = 1000;            // hand-mapped field (data/Fields.tsv)
auto Angle = Player->Get(RE::PROP::uPlayer::mAngle);      // a property: calls the game's getter
Player->Set(RE::PROP::uPlayer::mAngle, Angle);            // ... and its setter (false when read-only)
```

`include/RE/IDs_Fields.h` is generated by `tools/gen_fields.py` from the `tools/PropertyDump` dumps (the engine's own reflection: name, type, offset or accessors) and `data/Fields.tsv` (state the engine does not reflect, such as health and the inventory). The accessor conventions were checked in the disassembly and against the `ret N` of every getter and setter: scalars are `T get()` / `void set(T)`, structures (vectors, matrices, colors) go through a pointer (`void get(T* out)` / `void set(const T*)`), and array properties take a trailing index. Accessors run on the object that registered the property (`MtProperty::owner`), which can be a sub-object of the unit (`uModel::RenderInfo`), so `tools/PropertyDump` records it and only properties with a known owner get an id.

## Project Layout

```
include/MTFE/        MTFE API: PluginAPI.h (the C ABI shared with the loader), API.h, Log.h, Menu.h
include/REL/         relocations, hooks, pattern scans
include/RE/MT/       engine: MtDTI, MtObject, MtProperty, sMain, systems, cUnit / sUnit
include/RE/Game/     game: characters, damage, game info, items, input
include/RE/IDs_*.h   generated: DTI and vtable addresses of every engine class, function addresses, vtable slots
src/                 implementations
data/                Functions.tsv (generated), Names.tsv and VFuncs.tsv (hand-given names), Fields.tsv (hand-mapped fields)
examples/HelloRE5/   example plugin: every message, the systems, a match report and a settings page
tools/gen_ids.py     regenerates IDs_DTI.h, IDs_VTABLE.h and IDs_Classes.h from a class dump of the executable
tools/gen_functions.py  regenerates the function database and IDs_Functions.h / IDs_VFuncs.h
tools/gen_fields.py  regenerates IDs_Fields.h from the property dumps and Fields.tsv
tools/ghidra/        Ghidra scripts: export functions and vtables, apply the database's names
tools/gen_items.py   regenerates ItemNames.h
tools/PropertyDump/  development plugin: dumps the MT properties of every class or of live objects
tools/DamageTrace/   development plugin: logs every health change with its callers
```

## License

[GPL-3.0](LICENSE) with the [modding exception](EXCEPTIONS).
