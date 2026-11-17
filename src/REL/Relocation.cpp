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

#include "REL/Relocation.h"

#include "MTFE/PluginAPI.h"

#include <Windows.h>

#include <vector>

namespace REL
{
    Module::Module()
    {
        _base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        const auto* Dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(_base);
        const auto* Nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(_base + Dos->e_lfanew);
        const auto* Text = IMAGE_FIRST_SECTION(Nt);
        _timestamp = Nt->FileHeader.TimeDateStamp;
        _textBegin = _base + Text->VirtualAddress;
        _textEnd = _textBegin + Text->Misc.VirtualSize;
    }

    const Module& Module::Get()
    {
        static const Module Instance;
        return Instance;
    }

    bool Module::IsSupported() const
    {
        return _timestamp == MTFE_RUNTIME_RE5_STEAM_2023;
    }

    void SafeWrite(std::uintptr_t Address, const void* Data, std::size_t Size)
    {
        DWORD OldProtect = 0;
        VirtualProtect(reinterpret_cast<void*>(Address), Size, PAGE_EXECUTE_READWRITE, &OldProtect);
        std::memcpy(reinterpret_cast<void*>(Address), Data, Size);
        VirtualProtect(reinterpret_cast<void*>(Address), Size, OldProtect, &OldProtect);
        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(Address), Size);
    }

    void SafeFill(std::uintptr_t Address, std::uint8_t Value, std::size_t Count)
    {
        DWORD OldProtect = 0;
        VirtualProtect(reinterpret_cast<void*>(Address), Count, PAGE_EXECUTE_READWRITE, &OldProtect);
        std::memset(reinterpret_cast<void*>(Address), Value, Count);
        VirtualProtect(reinterpret_cast<void*>(Address), Count, OldProtect, &OldProtect);
        FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(Address), Count);
    }

    std::uintptr_t FollowRel32(std::uintptr_t Address)
    {
        std::int32_t Rel = 0;
        std::memcpy(&Rel, reinterpret_cast<const void*>(Address + 1), 4);
        return Address + 5 + Rel;
    }

    namespace
    {
        std::uintptr_t WriteRel32(std::uintptr_t Address, std::uintptr_t Target, std::uint8_t Opcode)
        {
            const auto Current = *reinterpret_cast<const std::uint8_t*>(Address);
            const auto Previous = (Current == 0xE8 || Current == 0xE9) ? FollowRel32(Address) : 0;
            std::uint8_t Patch[5] = { Opcode };
            const auto Rel = static_cast<std::int32_t>(Target - (Address + 5));
            std::memcpy(Patch + 1, &Rel, 4);
            SafeWrite(Address, Patch, sizeof(Patch));
            return Previous;
        }
    }

    std::uintptr_t WriteCall5(std::uintptr_t Address, std::uintptr_t Target)
    {
        return WriteRel32(Address, Target, 0xE8);
    }

    std::uintptr_t WriteBranch5(std::uintptr_t Address, std::uintptr_t Target)
    {
        return WriteRel32(Address, Target, 0xE9);
    }

    std::optional<std::uintptr_t> FindPattern(std::string_view Pattern, std::uintptr_t Begin, std::uintptr_t End)
    {
        std::vector<int> Bytes; // -1 = wildcard
        for (std::size_t i = 0; i < Pattern.size();)
        {
            if (Pattern[i] == ' ')
            {
                ++i;
                continue;
            }
            if (Pattern[i] == '?')
            {
                Bytes.push_back(-1);
                i += (i + 1 < Pattern.size() && Pattern[i + 1] == '?') ? 2 : 1;
                continue;
            }
            auto Hex = [](char Character)
            { return Character <= '9' ? Character - '0' : (Character | 0x20) - 'a' + 10; };
            Bytes.push_back(Hex(Pattern[i]) * 16 + Hex(Pattern[i + 1]));
            i += 2;
        }
        if (Bytes.empty() || End - Begin < Bytes.size())
        {
            return std::nullopt;
        }
        const auto* Memory = reinterpret_cast<const std::uint8_t*>(Begin);
        const auto Last = End - Begin - Bytes.size();
        for (std::size_t i = 0; i <= Last; ++i)
        {
            std::size_t k = 0;
            while (k < Bytes.size() && (Bytes[k] < 0 || Memory[i + k] == Bytes[k]))
            {
                ++k;
            }
            if (k == Bytes.size())
            {
                return Begin + i;
            }
        }
        return std::nullopt;
    }

    std::optional<std::uintptr_t> FindPattern(std::string_view Pattern)
    {
        const auto& Game = Module::Get();
        return FindPattern(Pattern, Game.GetTextBegin(), Game.GetTextEnd());
    }
}
