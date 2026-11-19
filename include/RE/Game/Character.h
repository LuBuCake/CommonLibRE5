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

#include "RE/IDs_Fields.h"
#include "RE/IDs_Functions.h"
#include "RE/IDs_VFuncs.h"
#include "RE/MT/cUnit.h"

namespace RE
{
    // uWork: uBioModel with the game's work data. Chain: uPlayer/uEnemy -> uCharacter -> uPawn -> uWork -> uBioModel
    // -> uModel -> uCoord -> cUnit.
    // Methods call the game: virtuals through the slot (RE::VFUNC, so the override of the object's class runs), the
    // others through the address (RE::ID). Both are named in data/VFuncs.tsv / data/Names.tsv (tools/gen_functions.py).
    class uPawn : public uModel
    {
    public:
        static const MtDTI* DTI() { return REL::Relocation<const MtDTI*>(RE::DTI::uPawn).Get(); }

        // Virtual HP setters, clamped to [0, max]. uPlayer overrides all three (damage feedback, flags at +0x2D8C).
        static constexpr std::size_t kSetHp = VFUNC::uPawn::SetHp; // 67: uPawn 0x84F740, uPlayer 0xB7C860
        static constexpr std::size_t kAddHp = VFUNC::uPawn::AddHp; // 68: uPawn 0x84F780, uPlayer 0xB7C8D0
        static constexpr std::size_t kSubHp = VFUNC::uPawn::SubHp; // 69: uPawn 0x84F7C0, uPlayer 0xB7C950

        void SetHp(std::int16_t Value) { REL::InvokeVFunc<void>(this, VFUNC::uPawn::SetHp, Value); }
        void AddHp(std::int16_t Value) { REL::InvokeVFunc<void>(this, VFUNC::uPawn::AddHp, Value); }
        void SubHp(std::int16_t Value) { REL::InvokeVFunc<void>(this, VFUNC::uPawn::SubHp, Value); }

        // vfunc 66: damage with the source id (players: difficulty scale; ignored when AIControlled() == 2). Returns
        // true if it died. RE/Game/Damage.h has the whole pipeline.
        bool Damage(std::int16_t Amount, std::int32_t Source = 0) { return REL::InvokeVFunc<bool>(this, VFUNC::uPawn::Damage, Amount, Source); }
    };

    // Fields go through RE::FIELD ids (these hand-mapped ones from data/Fields.tsv; the engine's own MT properties are
    // generated for every class), so Player->Hp() and Player->Get(RE::FIELD::uCharacter::Hp) are the same field.
    class uCharacter : public uPawn
    {
    public:
        static const MtDTI* DTI() { return REL::Relocation<const MtDTI*>(RE::DTI::uCharacter).Get(); }

        std::int16_t& Hp() { return Get(FIELD::uCharacter::Hp); }                             // [code][rt] player 1000, Majini 800
        std::int16_t& HpMax() { return Get(FIELD::uCharacter::HpMax); }                       // [code][rt]
        std::int32_t& LastDamageSource() { return Get(FIELD::uCharacter::LastDamageSource); } // [code] set by TakeDamage (vfunc 66)
        // Bit masks set to constants all over the code (0x0F/0x10/0x1F/0x70...). Player: 0x70 / 1, enemies: - / 0x10.
        std::uint32_t& HitReactionMask() { return Get(FIELD::uCharacter::HitReactionMask); } // [rt] CE "Attack Reaction"
        std::uint32_t& DamageMask() { return Get(FIELD::uCharacter::DamageMask); }           // [rt] GameX/CE: 0 = invulnerable
        float& MoveSpeed() { return Get(FIELD::uCharacter::MoveSpeed); }                     // [rt] 1.0 [CE] "Speed"

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

        // [code][rt] helpers of the fire action. __stdcall with the item as argument: they end in "ret 4" / "ret 8"
        // (declaring them __cdecl unbalanced the stack and crashed HelloRE5 with a /GS failure, 2026-11-14).
        bool IsInfiniteAmmo()
        {
            using Func = bool(__stdcall*)(cItem*);
            static REL::Relocation<Func> Call{ ID::cItem::IsInfiniteAmmo };
            return Call(this);
        }

        bool HasAmmo() // num > 0 or infinite
        {
            using Func = bool(__stdcall*)(cItem*);
            static REL::Relocation<Func> Call{ ID::cItem::HasAmmo };
            return Call(this);
        }

        std::int32_t ConsumeAmmo(std::int32_t Count) // num -= Count
        {
            using Func = std::int32_t(__stdcall*)(cItem*, std::int32_t);
            static REL::Relocation<Func> Call{ ID::cItem::ConsumeAmmo };
            return Call(this, Count);
        }

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

        cItem* Items() { return &Get(FIELD::uPlayer::Items); } // [prop][rt] = Inventory::items
        cItem& Item(std::size_t Index) { return Items()[Index]; }
        std::uint8_t& WeaponMode() { return Get(FIELD::uPlayer::WeaponMode); }        // [rt][GameX] 0 male / 1 female set
        std::uint8_t& Handedness() { return Get(FIELD::uPlayer::Handedness); }        // [rt][GameX] 0 right
        std::uint32_t& MeleeMove() { return Get(FIELD::uPlayer::MeleeMove); }         // [CE] current melee/move id
        std::uint32_t& MeleeActive() { return Get(FIELD::uPlayer::MeleeActive); }     // [CE]
        float& ModelScale() { return Get(FIELD::uPlayer::ModelScale); }               // [rt] 1.0 [CE] "Size"
        uPlayer*& Partner() { return Get(FIELD::uPlayer::Partner); }                  // [rt] null when solo [GameX]
        MtVector3& MovePos() { return Get(FIELD::uPlayer::MovePos); }                 // [rt] [CE] "moving coordinate"
        uCharacter*& MeleeTarget() { return Get(FIELD::uPlayer::MeleeTarget); }       // [CE]
        // Who drives the player: [rt] 0 = this machine's player [CE] 1 = AI [code] 2 = the online partner (the speech
        // that comes from the network plays only for it, 0x786970; PlayerDamage, vfunc 66, ignores all its damage).
        std::uint32_t& AIControlled() { return Get(FIELD::uPlayer::AIControlled); }
        std::uint32_t& HpChangeFlags() { return Get(FIELD::uPlayer::HpChangeFlags); } // [code] SubHp: |1 lost, |2 gained
        std::uint8_t& PlayerIndex() { return Get(FIELD::uPlayer::PlayerIndex); }      // [code] index into the difficulty/damage tables
        std::uint32_t& Speaking() { return Get(FIELD::uPlayer::Speaking); }           // [rt][GameX] 1 while a line plays

        // Says a voice line (the ids the game and the Vocalizer use: 16 "Go!", 66 low ammo, 82 handgun...), as the game
        // does: sPlayer::PlaySpeech, and to the online partner too when Network (does nothing offline). [code]
        void Speak(std::uint16_t Line, bool Network = true)
        {
            using Func = void(__thiscall*)(uPlayer*, std::uint32_t, std::int32_t, std::uint32_t, bool);
            static REL::Relocation<Func> Call{ ID::uPlayer::Speak };
            Call(this, Line, 0, 1, Network);
        }
    };

    class sPlayer : public cSystem
    {
    public:
        static constexpr std::size_t kMaxPlayers = 4;

        static const MtDTI* DTI() { return REL::Relocation<const MtDTI*>(RE::DTI::sPlayer).Get(); }
        static sPlayer* GetSingleton() { return *REL::Relocation<sPlayer**>(REL::Offset{ 0xDB27DC }); }

        uPlayer* GetPlayer(std::size_t Index) { return Get(FIELD::sPlayer::Players)[Index]; } // [rt][GameX]
        std::uint32_t GetPlayerCount() { return Get(FIELD::sPlayer::PlayerCount); }           // [rt] 1 solo [GameX]
        // The local player (index 0), or null outside a match: RE::sPlayer::GetSingleton()->GetLocalPlayer()->SetHp(1000).
        static uPlayer* GetLocalPlayer()
        {
            auto* Players = GetSingleton();
            return Players && Players->GetPlayerCount() ? Players->GetPlayer(0) : nullptr;
        }
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

        std::int32_t& GameMode() { return Get(FIELD::cGameSys::GameMode); }                // [code][rt] 5 = Mercenaries Reunion (2/5 = Mercenaries)
        std::uint32_t& LocalPlayerMask() { return Get(FIELD::cGameSys::LocalPlayerMask); } // [rt] bit i = player i is local [GameX]
        std::int32_t* PlayerStates() { return Get(FIELD::cGameSys::PlayerStates); }        // [code][rt] [4], 2 = in the match
        float& MercenariesTime() { return Get(FIELD::cGameSys::MercenariesTime); }         // [code][rt] copy of the timer, seconds left
        std::uint32_t& MercenariesSlot() { return Get(FIELD::cGameSys::MercenariesSlot); } // [CE][rt] 3
        std::int32_t& Score() { return Get(FIELD::cGameSys::Score); }                      // [CE] (0 at the start, not seen change)
    };
}
