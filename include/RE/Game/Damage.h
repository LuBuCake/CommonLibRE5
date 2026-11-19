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

// Damage pipeline (Steam 2023). Mapped 2026-11-14 with tools/DamageTrace (chapter 6-3 + Mercenaries) and Ghidra.
//
// Enemies (attacks from players/partners):
//   per frame  0x963C90  collects the HitInfos of the enemy (+0x129C and 4 more at +0x1BFC), keeps the strongest (+0x28)
//   OnHit      0x9E0840  base = vfunc 131 CalcDamage(hit) (per class: Majini 0x860450), then ApplyDamage
//   ApplyDamage 0x9DEA50 vfunc 129 CanTakeDamage(part, dmg) -> aGame difficulty scale (0x71B010) ->
//                        kill flag (hit->resultFlags & kForceKill: dmg = max HP) -> vfunc 69 SubHp -> death (vfunc 67)
//   ApplyDamage has ONE call site (0x9E08C7): WriteCall5 there sees every enemy hit and can change the damage.
// Players (attacks from enemies):
//   vfunc 66 Damage(short dmg, int source) -- enemy attack code calls it (or the base directly).
//     uPl* override 0xB64EB0: no damage if AIControlled == 2; aGame per-player scale (0x71AF90, player index +0x2D9C);
//     Mercenaries (mode 2/5): x0.7; other cases x0.5 / x0.8; minimum 1; then the base.
//     base (uPawn/uCharacter) 0xB56FB0 TakeDamage: lastDamageSource (+0x1368) = source; vfunc 69 SubHp; returns dead.
// Healing items: sPlayer::UseItem 0x7E9B70 -> 0x7E74C0(user, target, slot, sharePartner): item definition table
//   0x10EACF0 [id >> 8][id & 0xFF], 0x48 bytes each, heal amount (float) at +0x18; category 3 = healing items.

#include "RE/Game/Character.h"

namespace RE
{
    // Hit record passed to OnHit/ApplyDamage/CalcDamage. Field meanings from the trace; names are ours.
    struct HitInfo
    {
        enum ResultFlags : std::uint32_t // +0x3C
        {
            kForceKill = 0x10000000, // ApplyDamage uses the target's max HP as damage
        };

        MtVector3 direction;        // 00 normalized hit direction (rt: (-1,0,0), (0,0,1)...)
        std::uint8_t unk10[0x18];   // 10
        std::int32_t damage;        // 28 base damage before class/difficulty scaling (rt: 225..600; 3600 = one-shot)
        std::uint8_t unk2C[0x8];    // 2C
        float impact;               // 34 (rt: 0.1 melee, 0.5 guns, 5.0 some partner hits)
        float reaction;             // 38 stagger power (rt: 0, 1, 4, 8)
        std::uint32_t resultFlags;  // 3C ResultFlags (rt: 0x00800010 guns, 0x00080000 melee)
        std::uint32_t attackFlags;  // 40 attack type (rt: 0x2/0x6 guns, 0x200 melee; code: 0x400, 0x8000 = x6 vs some)
        std::uint32_t attackFlags2; // 44
        bool valid;                 // 48
        std::uint8_t part;          // 49 collision part of the target; category = per-model table (0xC44150)
    };
    static_assert(offsetof(HitInfo, damage) == 0x28);
    static_assert(offsetof(HitInfo, part) == 0x49);

    namespace Damage
    {
        // vfuncs (character vtables), named in data/VFuncs.tsv
        inline constexpr std::size_t kDamage = VFUNC::uPawn::Damage;               // 66: bool (int16 dmg, int source), true = died
        inline constexpr std::size_t kCanTakeDamage = VFUNC::uEnemy::CanTakeDamage; // 129: enemies, bool (uint8 part, int dmg)
        inline constexpr std::size_t kCalcDamage = VFUNC::uEnemy::CalcDamage;       // 131: enemies, int (HitInfo*)

        // Functions (data/Names.tsv). The old names stay as aliases.
        inline constexpr REL::Offset EnemyApplyDamage = ID::uEnemy::ApplyDamage; // __thiscall(uEnemy*, HitInfo*, int baseDamage)
        inline constexpr REL::Offset EnemyApplyDamageCall{ 0x5E08C7 };           // its one call site, inside EnemyOnHit
        inline constexpr REL::Offset EnemyOnHit = ID::uEnemy::OnHit;             // __thiscall(uEnemy*, HitInfo*)
        inline constexpr REL::Offset PlayerDamage = ID::uPlayer::Damage;         // uPlayer's vfunc 66
        inline constexpr REL::Offset TakeDamage = ID::uPawn::Damage;             // uPawn's vfunc 66
        inline constexpr REL::Offset UseItem = ID::sPlayer::UseItem;
        inline constexpr REL::Offset ItemTable{ 0xCEACF0 }; // ItemDef* [category][index], 0x48 each

        // Hooks the single EnemyApplyDamage call (chain-preserving). The hook receives (enemy, hit, baseDamage) and must
        // call `Original` with the (possibly changed) damage. Returns false if the call site is not the expected one.
        using EnemyApplyDamage_t = void(__thiscall*)(uCharacter*, HitInfo*, std::int32_t);
        bool HookEnemyApplyDamage(void(__fastcall* Hook)(uCharacter*, void*, HitInfo*, std::int32_t), EnemyApplyDamage_t& Original);

        // Direct damage through vfunc 66 (players: difficulty scaling applies). Returns true if the target died.
        inline bool Apply(uPawn* Target, std::int16_t Amount, std::int32_t Source = 0)
        {
            return Target->Damage(Amount, Source);
        }
    }
}
