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

// Characters (players and enemies). Source of each field in the comment:
//   [prop] MT property (live dump, 2026-11-14)          [code] read in the decompiled code (Ghidra)
//   [rt]   confirmed in game (Mercenaries Reunion, Excella, 12 enemies)   [CE]/[GameX] old tools, not re-checked

#include "RE/MT/cUnit.h"

namespace RE
{
    // uWork: uBioModel with the game's work data. Chain: uPlayer/uEnemy -> uCharacter -> uPawn -> uWork -> uBioModel
    // -> uModel -> uCoord -> cUnit.
    class uPawn : public uModel
    {
    public:
        static const MtDTI* DTI() { return REL::Relocation<const MtDTI*>(RE::DTI::uPawn).Get(); }

        // Virtual HP setters, clamped to [0, max]. uPlayer overrides Add/Sub (damage feedback, flags at +0x2D8C).
        static constexpr std::size_t kSetHp = 67; // [code] uPawn 0x84F740
        static constexpr std::size_t kAddHp = 68; // [code] uPawn 0x84F780, uPlayer 0xB7C8D0
        static constexpr std::size_t kSubHp = 69; // [code] uPawn 0x84F7C0, uPlayer 0xB7C950

        void SetHp(std::int16_t Value) { REL::InvokeVFunc<void>(this, kSetHp, Value); }
        void AddHp(std::int16_t Value) { REL::InvokeVFunc<void>(this, kAddHp, Value); }
        void SubHp(std::int16_t Value) { REL::InvokeVFunc<void>(this, kSubHp, Value); }
    };

    class uCharacter : public uPawn
    {
    public:
        static const MtDTI* DTI() { return REL::Relocation<const MtDTI*>(RE::DTI::uCharacter).Get(); }

        std::int16_t& Hp() { return Field<std::int16_t>(this, 0x1364); }               // [code][rt] player 1000, Majini 800
        std::int16_t& HpMax() { return Field<std::int16_t>(this, 0x1366); }            // [code][rt]
        std::int32_t& LastDamageSource() { return Field<std::int32_t>(this, 0x1368); } // [code] set by TakeDamage (vfunc 66)
        // Bit masks set to constants all over the code (0x0F/0x10/0x1F/0x70...). Player: 0x70 / 1, enemies: - / 0x10.
        std::uint32_t& HitReactionMask() { return Field<std::uint32_t>(this, 0x1358); } // [rt] CE "Attack Reaction"
        std::uint32_t& DamageMask() { return Field<std::uint32_t>(this, 0x135C); }      // [rt] GameX/CE: 0 = invulnerable
        float& MoveSpeed() { return Field<float>(this, 0x3A4); }                        // [rt] 1.0 [CE] "Speed"

        bool IsDead() { return Hp() <= 0; }
    };

    class uEnemy : public uCharacter
    {
    public:
        static const MtDTI* DTI() { return REL::Relocation<const MtDTI*>(RE::DTI::uEnemy).Get(); }
    };

    // Inventory slot (MT class cItem, 0x30). All fields are its MT properties except +0x2C.
    class cItem : public MtObject
    {
    public:
        static const MtDTI* DTI() { return REL::Relocation<const MtDTI*>(RE::DTI::cItem).Get(); }

        // members
        std::uint32_t id;           // 04 Id (item/weapon id, e.g. 286)
        std::int32_t num;           // 08 Num (ammo loaded / quantity)
        std::int32_t max;           // 0C Max
        std::uint32_t unk10;        // 10
        std::uint32_t unk14;        // 14
        std::uint32_t slotNo;       // 18 SlotNo
        std::uint32_t equipType;    // 1C EquipType
        std::uint8_t lvPower;       // 20 LvPower
        std::uint8_t lvFireSpeed;   // 21 LvFireSpeed (GameX "rapid fire")
        std::uint8_t lvReloadSpeed; // 22 LvReloadSpeed
        std::uint8_t lvBulletMax;   // 23 LvBulletMax (GameX "capacity"; +0x80 = infinite ammo)
        std::uint8_t lvBulletRate;  // 24 LvBulrRate
        std::uint8_t lvCritical;    // 25 LvCritical
        std::uint8_t lvThrough;     // 26 LvThrough (piercing)
        std::uint8_t lvRange;       // 27 LvRange
        std::uint8_t lvScope;       // 28 LvScope
        bool alive;                 // 29 Alive
        std::uint16_t pad2A;        // 2A
        uModel* weapon;             // 2C [rt] the weapon unit (uWp*) of this slot, or null
    };
    static_assert(sizeof(cItem) == 0x30);

    class uPlayer : public uCharacter
    {
    public:
        static constexpr std::size_t kItemCount = 24; // [code] Inventory (RE/Game/Items.h): 24 cItem; GameX showed 11

        static const MtDTI* DTI() { return REL::Relocation<const MtDTI*>(RE::DTI::uPlayer).Get(); }

        cItem* Items() { return &Field<cItem>(this, 0x21A4); } // [prop][rt] = Inventory::items
        cItem& Item(std::size_t Index) { return Items()[Index]; }
        std::uint8_t& WeaponMode() { return Field<std::uint8_t>(this, 0x1AF8); }      // [rt][GameX] 0 male / 1 female set
        std::uint8_t& Handedness() { return Field<std::uint8_t>(this, 0x1AF9); }      // [rt][GameX] 0 right
        std::uint32_t& MeleeMove() { return Field<std::uint32_t>(this, 0x10E0); }     // [CE] current melee/move id
        std::uint32_t& MeleeActive() { return Field<std::uint32_t>(this, 0x10EC); }   // [CE]
        float& ModelScale() { return Field<float>(this, 0x1E10); }                    // [rt] 1.0 [CE] "Size"
        uPlayer*& Partner() { return Field<uPlayer*>(this, 0x2A74); }                 // [rt] null when solo [GameX]
        MtVector3& MovePos() { return Field<MtVector3>(this, 0x2AD0); }               // [rt] [CE] "moving coordinate"
        uCharacter*& MeleeTarget() { return Field<uCharacter*>(this, 0x2DA4); }       // [CE]
        std::uint32_t& AIControlled() { return Field<std::uint32_t>(this, 0x2DA8); }  // [rt] 0 for the local player [CE] 1 = AI
        std::uint32_t& HpChangeFlags() { return Field<std::uint32_t>(this, 0x2D8C); } // [code] SubHp: |1 lost, |2 gained
        std::uint8_t& PlayerIndex() { return Field<std::uint8_t>(this, 0x2D9C); }     // [code] index into the difficulty/damage tables
                                                                                      // [code] PlayerDamage (vfunc 66) ignores all damage when AIControlled() == 2.
    };

    class sPlayer : public cSystem
    {
    public:
        static constexpr std::size_t kMaxPlayers = 4;

        static const MtDTI* DTI() { return REL::Relocation<const MtDTI*>(RE::DTI::sPlayer).Get(); }
        static sPlayer* GetSingleton() { return *REL::Relocation<sPlayer**>(REL::Offset{ 0xDB27DC }); }

        uPlayer* GetPlayer(std::size_t Index) { return Field<uPlayer*>(this, 0x24 + 4 * Index); } // [rt][GameX]
        std::uint32_t GetPlayerCount() { return Field<std::uint32_t>(this, 0x34); }               // [rt] 1 solo [GameX]
    };

    // Game mode / match state (sMain::mpGameSys, MT class cGameSys). Points into .text at the title screen.
    class cGameSys : public MtObject
    {
    public:
        static const MtDTI* DTI() { return REL::Relocation<const MtDTI*>(RE::DTI::cGameSys).Get(); }

        static cGameSys* GetSingleton()
        {
            auto* Main = sMain::GetSingleton();
            auto* GameSys = Main ? Field<cGameSys*>(Main, sMain::kGameSys) : nullptr;
            return GameSys && GameSys->IsKindOf(DTI()) ? GameSys : nullptr;
        }

        std::int32_t& GameMode() { return Field<std::int32_t>(this, 0x58); }           // [code][rt] 5 = Mercenaries Reunion (2/5 = Mercenaries)
        std::uint32_t& LocalPlayerMask() { return Field<std::uint32_t>(this, 0x47C); } // [rt] bit i = player i is local [GameX]
        std::int32_t* PlayerStates() { return &Field<std::int32_t>(this, 0x620); }     // [code][rt] [4], 2 = in the match
        float& MercenariesTime() { return Field<float>(this, 0x680); }                 // [code][rt] copy of the timer, seconds left
        std::uint32_t& MercenariesSlot() { return Field<std::uint32_t>(this, 0x600); } // [CE][rt] 3
        std::int32_t& Score() { return Field<std::int32_t>(this, 0x6B4); }             // [CE] (0 at the start, not seen change)
    };
}
