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

// Save / game state, all inside sInfoMan (global 0x11B2158). Sources: [prop] MT property of the game, [rt] read in game
// (2026-11-14: Mercenaries Reunion with Barry then Josh, same map; campaign Chris Heavy Metal + Sheva Full Armor),
// [GameX] old trainer layout (its base + 0x6FE00 = cGameInfo + 0x100: same record layout, re-checked in game).

#include "RE/IDs_DTI.h"
#include "RE/MT/sMain.h"

#include <string_view>

namespace RE
{
    enum class CharacterID : std::int32_t // = the hex number in the player class name (uPl05Excella -> 5)
    {
        kChris = 0,
        kSheva = 1,
        kJill = 2,
        kWesker = 3,
        kJosh = 4,
        kExcella = 5,
        kBarry = 6,
        kRebecca = 7,
        kIrving = 0x86,
    };

    struct CostumeInfo
    {
        CharacterID Character;
        std::int32_t Costume;
        const char* PlayerClass; // the uPl* class the game creates
        const char* Name;        // GameX names
    };

    // [GameX][rt] (Chris 9 = uPl00ChrisCos4, Sheva 3 = uPl01ShebaArmorF, Josh 1 = uPl04JoshCos2 seen in game).
    inline constexpr CostumeInfo kCostumes[] = {
        { CharacterID::kChris, 0, "uPl00ChrisNormal", "BSAA" },
        { CharacterID::kChris, 1, "uPl00ChrisArmorA", "Melee Vest" },
        { CharacterID::kChris, 2, "uPl00ChrisArmorB", "Bulletproof Vest" },
        { CharacterID::kChris, 3, "uPl00ChrisArmorF", "Full Armor" },
        { CharacterID::kChris, 4, "uPl00ChrisPlain", "Plain" },
        { CharacterID::kChris, 5, "uPl00ChrisCos1", "Safari" },
        { CharacterID::kChris, 6, "uPl00ChrisCos2", "STARS" },
        { CharacterID::kChris, 7, "uPl00ChrisCos3", "Warrior" },
        { CharacterID::kChris, 8, "uPl00ChrisEp1", "Lost in Nightmares" },
        { CharacterID::kChris, 9, "uPl00ChrisCos4", "Heavy Metal" },
        { CharacterID::kSheva, 0, "uPl01ShebaNormal", "BSAA" },
        { CharacterID::kSheva, 1, "uPl01ShebaArmorA", "Melee Vest" },
        { CharacterID::kSheva, 2, "uPl01ShebaArmorB", "Bulletproof Vest" },
        { CharacterID::kSheva, 3, "uPl01ShebaArmorF", "Full Armor" },
        { CharacterID::kSheva, 4, "uPl01ShebaPlain", "Plain" },
        { CharacterID::kSheva, 5, "uPl01ShebaCos1", "Amazon" },
        { CharacterID::kSheva, 6, "uPl01ShebaCos2", "Clubbin" },
        { CharacterID::kSheva, 7, "uPl01ShebaCos3", "Business" },
        { CharacterID::kSheva, 8, "uPl01ShebaCos4", "Fairy Tale" },
        { CharacterID::kJill, 0, "uPl02JillCos1", "BSAA" },
        { CharacterID::kJill, 1, "uPl02JillCos2", "Battlesuit" },
        { CharacterID::kJill, 2, "uPl02JillCos3", "Desperate Escape" },
        { CharacterID::kJill, 3, "uPl02JillCos4", "Lost in Nightmares" },
        { CharacterID::kWesker, 0, "uPl03WeskerCos1", "Midnight" },
        { CharacterID::kWesker, 1, "uPl03WeskerCos2", "STARS" },
        { CharacterID::kJosh, 0, "uPl04JoshCos1", "Story Mode" },
        { CharacterID::kJosh, 1, "uPl04JoshCos2", "Reunion BSAA" },
        { CharacterID::kExcella, 0, "uPl05ExcellaNormal", "Tricell" },
        { CharacterID::kBarry, 0, "uPl06BarryNormal", "STARS" },
        { CharacterID::kRebecca, 0, "uPl07RebeccaNormal", "STARS" },
        { CharacterID::kIrving, 0, "uPl86Irving", "Story Mode" },
    };

    const CostumeInfo* FindCostume(CharacterID Character, std::int32_t Costume);
    const CostumeInfo* FindCostumeByClass(std::string_view PlayerClass); // e.g. player->GetDTI()->GetName()

    // Character choice of one player for the next load: 0x50 bytes. [rt][GameX]
    struct PlayerRecord
    {
        std::uint32_t unk00;      // 00 1
        std::uint32_t unk04;      // 04 1
        CharacterID character;    // 08 [rt] Barry 6 -> Josh 4 between two Mercenaries matches
        std::int32_t costume;     // 0C [rt] kCostumes numbering
        std::int32_t hp;          // 10 [rt] 1000
        std::int32_t hpMax;       // 14 [rt] 1000
        std::uint8_t unk18[0x38]; // 18
    };
    static_assert(sizeof(PlayerRecord) == 0x50);

    // Inventory slot carried into a stage (loading screen inventory): 0x2C bytes. [rt] ids/num, [GameX] the rest.
    struct ItemRecord
    {
        std::uint32_t id;         // 00 [rt] same ids as cItem::id
        std::int32_t num;         // 04 [rt]
        std::int32_t unk08;       // 08
        std::uint8_t unk0C[0x10]; // 0C
        std::uint8_t lvPower;     // 1C [GameX] firepower
        std::uint8_t lvFireSpeed; // 1D
        std::uint8_t lvReload;    // 1E [GameX] reload speed
        std::uint8_t lvCapacity;  // 1F [GameX] capacity
        std::uint8_t unk20[0xC];  // 20
    };
    static_assert(sizeof(ItemRecord) == 0x2C);

    // Current game (MT class cGameInfo, 0x5940). Two of them in sInfoMan; sInfoMan::GetActiveGame() is the one the
    // code reads (index at sInfoMan+0x20).
    class cGameInfo : public MtObject
    {
    public:
        static constexpr std::size_t kRecords = 4;
        static constexpr std::size_t kItemSlots = 11; // [GameX] slot 9 = knife

        static const MtDTI* DTI() { return REL::Relocation<const MtDTI*>(RE::DTI::cGameInfo).Get(); }

        PlayerRecord* PlayerRecords() { return &Field<PlayerRecord>(this, 0x100); }                               // [rt] [4], Mercenaries/current
        PlayerRecord* StoryRecords() { return &Field<PlayerRecord>(this, 0x1690); }                               // [rt] [2], campaign (P1/P2)
        ItemRecord* LoadingItems(std::size_t Player) { return &Field<ItemRecord>(this, 0x240 + 0x420 * Player); } // [rt] P1, [GameX] stride
        ItemRecord* HistoryItems() { return &Field<ItemRecord>(this, 0x17D0); }                                   // [rt][GameX] campaign inventory

        // [prop] mTempBase.*
        std::uint32_t& NewGame() { return Field<std::uint32_t>(this, 0x41B0); }
        std::uint32_t& StartChara() { return Field<std::uint32_t>(this, 0x41B4); }
        std::uint32_t& Difficulty() { return Field<std::uint32_t>(this, 0x41B8); }
        std::uint32_t& GamePoint() { return Field<std::uint32_t>(this, 0x41BC); }
        std::uint32_t& ChapterNo() { return Field<std::uint32_t>(this, 0x41C0); }
        std::uint32_t& ChapterStart() { return Field<std::uint32_t>(this, 0x41C4); }
        std::uint32_t& StageNo() { return Field<std::uint32_t>(this, 0x41C8); } // the "area" the damage code checks (500, 0x2C2...)
        std::uint32_t& StartPosIdx() { return Field<std::uint32_t>(this, 0x41CC); }
        std::uint32_t& SceNo() { return Field<std::uint32_t>(this, 0x41D0); }
        std::uint32_t& OperationNo() { return Field<std::uint32_t>(this, 0x41D4); }

        struct ChapterStats // [prop] mTempChap[i], 0x18 per player
        {
            std::uint32_t dieNum;
            std::uint32_t enemyKillNum;
            std::uint32_t atkFireNum;
            std::uint32_t atkHitNum;
            float playTime;
            bool playTimeEnable;
        };
        ChapterStats& Stats(std::size_t Player) { return Field<ChapterStats>(this, 0x41D8 + 0x18 * Player); }
    };

    // Options + progress (MT class cSystemInfo, 0x8C74). All fields: [prop] mTmpData.*
    class cSystemInfo : public MtObject
    {
    public:
        static const MtDTI* DTI() { return REL::Relocation<const MtDTI*>(RE::DTI::cSystemInfo).Get(); }

        std::int32_t& PadReverse() { return Field<std::int32_t>(this, 0x20); }
        std::int32_t& PadSpeed() { return Field<std::int32_t>(this, 0x24); }
        std::int32_t& PadType() { return Field<std::int32_t>(this, 0x28); }
        std::int32_t& Brightness() { return Field<std::int32_t>(this, 0x2C); }
        std::int16_t& Subtitle() { return Field<std::int16_t>(this, 0x34); }
        std::int32_t& Language() { return Field<std::int32_t>(this, 0x38); }
        std::int32_t& BgmVolume() { return Field<std::int32_t>(this, 0x3C); }
        std::int32_t& SeVolume() { return Field<std::int32_t>(this, 0x40); }
        std::int32_t& Costume(std::size_t Index) { return Field<std::int32_t>(this, 0x44 + 4 * Index); } // mOption.mCostume[2]
        std::int32_t& Filter() { return Field<std::int32_t>(this, 0x4C); }
        std::int32_t& InfiniteAmmo() { return Field<std::int32_t>(this, 0x50); }
        std::int32_t& JoinType() { return Field<std::int32_t>(this, 0x54); }
        std::int32_t& FriendlyFire() { return Field<std::int32_t>(this, 0x58); }
        std::uint32_t& Complete() { return Field<std::uint32_t>(this, 0x110); }
        std::uint32_t& SpsCostume(std::size_t Index) { return Field<std::uint32_t>(this, 0x118 + 4 * Index); }
        std::uint32_t& TotalPlayTime() { return Field<std::uint32_t>(this, 0x180); }
        std::uint32_t& Money() { return Field<std::uint32_t>(this, 0x188); }
        std::uint32_t& Point() { return Field<std::uint32_t>(this, 0x18C); }
    };

    class sInfoMan : public cSystem
    {
    public:
        static const MtDTI* DTI() { return REL::Relocation<const MtDTI*>(RE::DTI::sInfoMan).Get(); }
        static sInfoMan* GetSingleton() { return *REL::Relocation<sInfoMan**>(REL::Offset{ 0xDB2158 }); }

        std::uint32_t& ActiveIndex() { return Field<std::uint32_t>(this, 0x20); }                           // [code][rt] 0
        cSystemInfo* System(std::size_t Index) { return &Field<cSystemInfo>(this, 0x38 + 0x8C74 * Index); } // [prop] mSystem[2]
        cGameInfo* Game(std::size_t Index) { return &Field<cGameInfo>(this, 0x23210 + 0x5940 * Index); }    // [prop] mGame[2]
        cGameInfo* GetActiveGame() { return Game(ActiveIndex() & 1); }
    };
}
