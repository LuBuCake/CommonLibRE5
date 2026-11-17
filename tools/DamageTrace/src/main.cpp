/*
    This file is part of CommonLibRE5.
    CommonLibRE5 is free software: you can redistribute it
    and/or modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation, either version 3 of
    the License, or (at your option) any later version.
    CommonLibRE5 is distributed in the hope that it will
    be useful, but WITHOUT ANY WARRANTY; without even the implied
    warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    See the GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with CommonLibRE5. If not, see <https://www.gnu.org/licenses/>.
*/

// DamageTrace (developer tool): logs every HP change of characters to <MTFE>\Logs\DamageTrace.log.
//   1. Hooks SetHp/AddHp/SubHp (vfunc 67/68/69) in the vtable of every class derived from uPawn (chain kept per
//      vtable): class, value, HP before/after and the probable return addresses found on the stack.
//   2. Every frame compares the HP of players and enemies: a change not seen by the hooks = direct write.

#include <RE/IDs_Classes.h>
#include <RE/RE5.h>
#include <MTFE/MTFE.h>

#include <Windows.h>
#include <intrin.h>

#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

#pragma intrinsic(_ReturnAddress, _AddressOfReturnAddress)

namespace
{
    using HpFn = void(__thiscall*)(void*, std::int16_t);

    struct Slots
    {
        std::uintptr_t Original[3]{}; // SetHp, AddHp, SubHp
    };

    std::unordered_map<std::uintptr_t, Slots> VTables; // vtable -> originals
    std::unordered_map<void*, std::int16_t> LastHp;    // last HP seen per character
    bool InHook = false;

    const char* NameOf(RE::MtObject* Object)
    {
        const auto* Dti = Object ? Object->GetDTI() : nullptr;
        return Dti ? Dti->GetName() : "?";
    }

    // Values on the stack that point into .text right after a call instruction. Bounded by the thread's stack base:
    // HP setters also run on loading threads with a shallow stack (reading past it crashed v1 in chapter 6-3).
    std::string StackCallers(const void* Top)
    {
        const auto& Game = REL::Module::Get();
        const auto* Stack = static_cast<const std::uintptr_t*>(Top);
        const auto* Tib = reinterpret_cast<const NT_TIB*>(NtCurrentTeb());
        const auto* End = static_cast<const std::uintptr_t*>(Tib->StackBase);
        std::string Out = std::format(" [tid {}]", GetCurrentThreadId());
        int Found = 0;
        for (int i = 0; i < 160 && Found < 10 && Stack + i < End; ++i)
        {
            const auto Value = Stack[i];
            if (Value < Game.GetTextBegin() + 6 || Value >= Game.GetTextEnd())
            {
                continue;
            }
            const auto* Code = reinterpret_cast<const std::uint8_t*>(Value);
            const bool CallRel = Code[-5] == 0xE8;
            const bool CallInd = Code[-2] == 0xFF || Code[-3] == 0xFF || Code[-6] == 0xFF;
            if (CallRel || CallInd)
            {
                Out += std::format(" {:08X}", Value);
                ++Found;
            }
        }
        return Out;
    }

    template <int Slot>
    void __fastcall Hook(RE::uCharacter* Self, void*, std::int16_t Value)
    {
        const auto VTable = Self->GetVTable();
        const auto It = VTables.find(VTable);
        if (It == VTables.end())
        {
            return;
        }
        const auto Before = Self->Hp();
        const bool Outer = !InHook;
        InHook = true;
        reinterpret_cast<HpFn>(It->second.Original[Slot])(Self, Value);
        InHook = !Outer ? InHook : false;
        const auto After = Self->Hp();
        LastHp[Self] = After;

        static constexpr const char* kNames[] = { "SetHp", "AddHp", "SubHp" };
        MTFE::Log::Info("{} {:08X} {} {} : {} -> {}/{} | ret {:08X} | stack{}", kNames[Slot],
                        reinterpret_cast<std::uintptr_t>(Self), NameOf(Self), Value, Before, After, Self->HpMax(),
                        reinterpret_cast<std::uintptr_t>(_ReturnAddress()), StackCallers(_AddressOfReturnAddress()));
    }

    RE::Damage::EnemyApplyDamage_t ApplyDamage = nullptr;

    void __fastcall HookApplyDamage(RE::uCharacter* Self, void*, RE::HitInfo* Hit, std::int32_t Damage)
    {
        if (Hit)
        {
            const auto& Direction = Hit->direction;
            MTFE::Log::Info("ApplyDamage {:08X} {} base {} | hit damage {} part {} result {:08X} attack {:08X} {:08X} impact {:.2f} "
                            "reaction {:.2f} valid {} dir ({:.0f},{:.0f},{:.0f})",
                            reinterpret_cast<std::uintptr_t>(Self), NameOf(Self), Damage, Hit->damage, Hit->part, Hit->resultFlags,
                            Hit->attackFlags, Hit->attackFlags2, Hit->impact, Hit->reaction, Hit->valid, Direction.x, Direction.y, Direction.z);
        }
        ApplyDamage(Self, Hit, Damage);
    }

    void InstallHooks()
    {
        if (RE::Damage::HookEnemyApplyDamage(&HookApplyDamage, ApplyDamage))
        {
            MTFE::Log::Info("hooked the enemy ApplyDamage call at {:08X}", RE::Damage::EnemyApplyDamageCall.GetAddress());
        }

        const auto Base = REL::Module::Get().GetBase();
        const auto* Pawn = RE::uPawn::DTI();
        std::size_t Classes = 0;
        for (const auto& Entry : RE::kClasses)
        {
            if (!Entry.VTable)
            {
                continue;
            }
            const auto* Dti = reinterpret_cast<const RE::MtDTI*>(Base + Entry.DTI);
            if (!Dti->IsKindOf(Pawn))
            {
                continue;
            }
            const auto VTable = Base + Entry.VTable;
            if (VTables.contains(VTable))
            {
                continue;
            }
            REL::Relocation<std::uintptr_t> Table{ VTable };
            Slots Originals;
            Originals.Original[0] = Table.WriteVFunc(RE::uPawn::kSetHp, &Hook<0>);
            Originals.Original[1] = Table.WriteVFunc(RE::uPawn::kAddHp, &Hook<1>);
            Originals.Original[2] = Table.WriteVFunc(RE::uPawn::kSubHp, &Hook<2>);
            VTables.emplace(VTable, Originals);
            ++Classes;
        }
        MTFE::Log::Info("hooked SetHp/AddHp/SubHp in {} uPawn vtables", Classes);
    }

    void Watch(RE::cUnit* Unit)
    {
        auto* Character = Unit->As<RE::uCharacter>();
        if (!Character)
        {
            return;
        }
        const auto Hp = Character->Hp();
        auto [It, Inserted] = LastHp.try_emplace(Character, Hp);
        if (!Inserted && It->second != Hp)
        {
            MTFE::Log::Warn("direct HP write {:08X} {} : {} -> {}/{} (not through SetHp/AddHp/SubHp)",
                            reinterpret_cast<std::uintptr_t>(Character), NameOf(Character), It->second, Hp, Character->HpMax());
            It->second = Hp;
        }
    }

    void OnFrame()
    {
        auto* Units = RE::sUnit::GetSingleton();
        if (!Units)
        {
            return;
        }
        for (auto Line : { RE::sUnit::kPlayer, RE::sUnit::kEnemy })
        {
            const auto WatchUnit = [](RE::cUnit* Unit)
            {
                Watch(Unit);
                return true;
            };
            Units->ForEachUnit(Line, WatchUnit);
        }
    }

    void OnMessage(MTFE::Message* Msg)
    {
        switch (Msg->type)
        {
        case MTFE_MESSAGE_PRE_WINMAIN:
            InstallHooks();
            break;
        case MTFE_MESSAGE_GAME_INIT:
            MTFE::AddFrameCallback(OnFrame);
            break;
        }
    }
}

MTFE_PLUGIN_LOAD(const MTFE::LoadInterface* Interface)
{
    MTFE::Init(Interface);
    return MTFE::RegisterListener(OnMessage);
}
