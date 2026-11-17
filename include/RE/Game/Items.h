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

// Items, weapons and the player inventory (Steam 2023). Sources: [prop] MT property, [code] Ghidra, [rt] in game.
//
// Item id = (category << 8) | index. Definitions (sParameter::ITEM_INFO_STRUCT, 0x48) per category from the pointer
// table 0x10EACF0 [8]: 0 none, 1 weapons (144, = WEAPON_INFO index = uWp<index> class), 2 ammo (18), 3 healing (19),
// 4..7 others. Weapons also have a sParameter::WEAPON_INFO_STRUCT (0x140) at 0x11B5170 [144].
//
// Firing (player fire action 0xC89F30, virtual): equipped item = Inventory::GetItem(kSlotEquipped); for a gun
// (ItemInfo::itemReadyX == 1): if !IsInfiniteAmmo(item) -> ConsumeAmmo(item, 1); if !HasAmmo -> weapon empty flag.

#include "RE/Game/Character.h"
#include "RE/Game/ItemNames.h"

namespace RE
{
    // sParameter::ITEM_INFO_STRUCT. All fields [prop]; names stripped in the release (mDebugName is "").
    struct ItemInfo
    {
        void* vtbl;                   // 00
        const char* debugName;        // 04
        std::int32_t num;             // 08 mNum
        std::int32_t max;             // 0C mMax
        std::int32_t numSub;          // 10
        std::int32_t maxSub;          // 14
        float value;                  // 18 mFFree0 (healing items: HP healed, [code] UseItem)
        bool slot;                    // 1C mbSlot
        bool equip;                   // 1D mbEquip
        bool itemGet;                 // 1E mbItemGet
        bool stack;                   // 1F mbStack
        std::int32_t throwNum;        // 20
        std::uint32_t itemReadyX;     // 24 [code] fire action: 1 = gun
        std::uint32_t itemReadyB;     // 28
        std::uint32_t itemExecX;      // 2C
        std::uint32_t itemExecB;      // 30
        std::uint32_t exSlotFlag;     // 34
        std::uint32_t exSlotParentId; // 38
        std::uint32_t equipOfsTblNo;  // 3C
        std::uint32_t nameMsgNo;      // 40 message id of the name
        std::uint32_t idDispFlag;     // 44
    };
    static_assert(sizeof(ItemInfo) == 0x48);

    // sParameter::WEAPON_INFO_STRUCT (0x140). [prop] fields; the rest stays raw.
    struct WeaponInfo
    {
        void* vtbl;                  // 000
        const char* debugName;       // 004 "WP1E HAND_93R" (kept in the release)
        std::uint32_t hitParamNo;    // 008
        float laserLength;           // 00C
        std::uint32_t burstMode;     // 010
        std::uint32_t bulletMode;    // 014 [code] 1 = never consumes ammo
        std::uint32_t reloadMode;    // 018
        std::uint32_t bulletId;      // 01C ammo item id
        float frameReload;           // 020
        float frameReloadCancel;     // 024
        std::uint8_t pad028[4];      // 028
        std::uint32_t cartOmId;      // 02C ejected cartridge object
        float frameCart;             // 030
        std::uint8_t pad034[0x50];   // 034 cartridge speed/rotation vectors
        float frameReadyBeginCancel; // 084
        bool laserSight;             // 088
        std::uint8_t pad089[0xF];    // 089
        std::uint32_t scopeType;     // 098
        bool knife;                  // 09C
        std::uint8_t pad09D[3];      // 09D
        float aimSpeed;              // 0A0
        float aimAngXUp;             // 0A4
        float aimAngXDw;             // 0A8
        float bulrSpeed;             // 0AC
        float bulrScale;             // 0B0
        float bulrConv;              // 0B4
        std::uint32_t wepGroup;      // 0B8
        std::uint32_t wepMat;        // 0BC
        std::uint32_t muzzleEffId;   // 0C0
        std::uint32_t muzzleEffType; // 0C4
        bool carry;                  // 0C8
        std::uint8_t pad0C9[0x47];   // 0C9 carry positions/angles, male/female (vectors at 0xD0..0x10F)
        std::uint8_t range[0xC];     // 110 mRange
        std::uint8_t decay[0xC];     // 11C mDecay
        std::uint32_t recoilShock;   // 128
        std::uint32_t vibListNo;     // 12C
        std::uint8_t pad130[0x10];   // 130
    };
    static_assert(sizeof(WeaponInfo) == 0x140);
    static_assert(offsetof(WeaponInfo, scopeType) == 0x98);
    static_assert(offsetof(WeaponInfo, recoilShock) == 0x128);

    namespace Items
    {
        inline constexpr REL::Offset CategoryTable{ 0xCEACF0 }; // ItemInfo* [8]
        inline constexpr REL::Offset WeaponTable{ 0xDB5170 };   // WeaponInfo[144]
        inline constexpr std::size_t kWeaponCount = 144;

        inline std::uint32_t Category(std::uint32_t ID)
        {
            return (ID >> 8) & 0xFF;
        }
        inline bool IsWeapon(std::uint32_t ID)
        {
            return (ID & 0xFF00) == 0x100;
        }

        const ItemInfo* GetItemInfo(std::uint32_t ID);     // nullptr for unknown ids
        const WeaponInfo* GetWeaponInfo(std::uint32_t ID); // weapons only
        const char* GetName(std::uint32_t ID);             // GameX name, else the weapon debug name, else nullptr

        // [code] helpers used by the fire action. __stdcall: they end in "ret 4" / "ret 8" (declaring them cdecl
        // unbalanced the stack and crashed HelloRE5 with a /GS failure, 2026-11-14).
        inline constexpr REL::Offset ConsumeAmmoFn{ 0x84C1E0 };    // int __stdcall (cItem*, int n): num -= n
        inline constexpr REL::Offset IsInfiniteAmmoFn{ 0x84C200 }; // bool __stdcall (cItem*)
        inline constexpr REL::Offset HasAmmoFn{ 0x84C2F0 };        // bool __stdcall (cItem*): num > 0 || infinite
        inline constexpr REL::Offset PlayerFireAction{ 0x889F30 }; // player fire action update (virtual)

        inline bool IsInfiniteAmmo(cItem* Item)
        {
            return REL::Relocation<bool(__stdcall*)(cItem*)>(IsInfiniteAmmoFn)(Item);
        }
        inline bool HasAmmo(cItem* Item)
        {
            return REL::Relocation<bool(__stdcall*)(cItem*)>(HasAmmoFn)(Item);
        }
        inline void ConsumeAmmo(cItem* Item, std::int32_t Count)
        {
            REL::Relocation<std::int32_t(__stdcall*)(cItem*, std::int32_t)>{ ConsumeAmmoFn }(Item, Count);
        }
    }

    // The player's inventory (uPlayer + 0x21A0). [code] 0xC4B700/0xC4DB20 loop over 24 cItem.
    class Inventory
    {
    public:
        static constexpr std::size_t kSlots = 24;
        static constexpr std::size_t kGridSlots = 9;            // [code] the fire action checks slots 0..8 for ammo left
        static constexpr std::uint32_t kSlotEquipped = 0xE;     // equipType == 1 (weapon in hand)
        static constexpr std::uint32_t kSlotEquippedItem = 0xF; // equipType == 2
        static constexpr std::uint32_t kSlotAutoWeapon = 0x10;  // weapon the game would pick

        inline static constexpr REL::Offset GetItemFn{ 0x84B700 }; // __thiscall(Inventory*, slot) -> cItem*
        inline static constexpr REL::Offset ConsumeFn{ 0x84DB20 }; // __thiscall(Inventory*, id, n) -> int left
        inline static constexpr REL::Offset CombineFn{ 0x84E640 }; // slot -> slot (ammo into gun, herbs)

        // Slot by SlotNo (0..23) or a kSlot* constant.
        cItem* GetItem(std::uint32_t Slot) { return REL::InvokeThiscall<cItem*>(GetItemFn.GetAddress(), this, Slot); }
        cItem* GetEquipped() { return GetItem(kSlotEquipped); }
        // Takes Count of item ID from the slots holding it (fewest first), clearing emptied slots. Returns what was missing.
        std::int32_t Consume(std::uint32_t ID, std::int32_t Count) { return REL::InvokeThiscall<std::int32_t>(ConsumeFn.GetAddress(), this, ID, Count); }

        // members
        std::uint32_t unk00; // 000
        cItem items[kSlots]; // 004
        cItem* lastAuto;     // 484 [code] cache of kSlotAutoWeapon
    };
    static_assert(offsetof(Inventory, lastAuto) == 0x484);

    inline Inventory* GetInventory(uPlayer* Player)
    {
        return &Field<Inventory>(Player, 0x21A0);
    }
}
