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

#include "RE/Game/Items.h"

namespace RE::Items
{
    namespace
    {
        // Entries per category: distance to the next category pointer (the table is contiguous, see Items.h).
        std::uint32_t CategoryCount(std::uint32_t CategoryIndex)
        {
            const auto* Table = REL::Relocation<const ItemInfo* const*>(CategoryTable).Get();
            if (CategoryIndex >= 8 || !Table[CategoryIndex])
            {
                return 0;
            }
            if (CategoryIndex == 7)
            {
                return 16; // 407 ITEM_INFO_STRUCT in the run at 0x10EAF30, 391 in categories 0..6 (dump, 2026-11-14)
            }
            return static_cast<std::uint32_t>(Table[CategoryIndex + 1] - Table[CategoryIndex]);
        }
    }

    const ItemInfo* GetItemInfo(std::uint32_t ID)
    {
        const auto CategoryIndex = Category(ID);
        const auto Index = ID & 0xFF;
        if (ID == 0xFFFF || Index >= CategoryCount(CategoryIndex))
        {
            return nullptr;
        }
        const auto* Table = REL::Relocation<const ItemInfo* const*>(CategoryTable).Get();
        return Table[CategoryIndex] + Index;
    }

    const WeaponInfo* GetWeaponInfo(std::uint32_t ID)
    {
        if (!IsWeapon(ID) || (ID & 0xFF) >= kWeaponCount)
        {
            return nullptr;
        }
        return REL::Relocation<const WeaponInfo*>(WeaponTable).Get() + (ID & 0xFF);
    }

    const char* GetName(std::uint32_t ID)
    {
        for (const auto& Entry : kItemNames)
        {
            if (Entry.ID == ID)
            {
                return Entry.Name;
            }
        }
        if (IsWeapon(ID) && (ID & 0xFF) < kWeaponCount)
        {
            return kWeaponDebugNames[ID & 0xFF];
        }
        return nullptr;
    }
}
