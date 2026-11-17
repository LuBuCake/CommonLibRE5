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

#pragma once

// Hand-verified addresses (RVA = Ghidra address - 0x400000), runtime Steam 2023. Each one: where it was confirmed.

#include "REL/Relocation.h"

namespace RE::Offset
{
    namespace sMain
    {
        // sMain::sMain stores `this` here right after its vftable (0x425F08: mov [0x12340A4], ebx).
        inline constexpr REL::Offset Singleton{ 0xE340A4 };
    }

    namespace MtDTI
    {
        // Parent of DTI::MtObject: the root of the class tree (no name).
        inline constexpr REL::Offset Root{ 0xDAF714 };
    }

    namespace MtProperty
    {
        inline constexpr REL::Offset AddProperty{ 0x22C00 }; // __thiscall(list, const MtProperty* entry)
        inline constexpr REL::Offset Alloc{ 0x2D26D0 };      // pops the free list or takes the next array slot
        inline constexpr REL::Offset FreeList{ 0xE496A8 };   // MtProperty* head, linked through +0x24
        inline constexpr REL::Offset PoolLock{ 0xE4C44C };   // CRITICAL_SECTION taken by Alloc
    }

    namespace MtFile
    {
        // __thiscall(MtFile*, const char* path, uint mode): CreateFileA; on failure calls FatalOpenError (no flag, no
        // fallback). Reached when a resource is not in the loaded ARCs and the game tries the loose file.
        inline constexpr REL::Offset Open{ 0x2F9700 };
        // Appends "Fail: '<path>' <err>" to re5LoadFails_jim.txt (game folder), then "Failed open file. %s %d" + exit.
        inline constexpr REL::Offset FatalOpenError{ 0x2F9690 };
    }

    namespace Entry
    {
        inline constexpr REL::Offset WinMain{ 0x313F40 }; // WinMain: app init ("RESIDENT EVIL 5", "MTFramework", &sBioMain::DTI) + main loop
        inline constexpr REL::Offset MainLoop{ 0x23990 }; // app main loop: PeekMessage + sMain->vfunc[6] per frame
    }
}
