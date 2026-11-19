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

// Units: everything updated by sUnit (characters, weapons, objects, cameras, lights, effects...).
// Offsets: MT properties (live dump of the units of a Mercenaries match, 2026-11-14) unless noted.

#include "RE/IDs_DTI.h"
#include "RE/MT/MtMath.h"
#include "RE/MT/sMain.h"

#include <cstdint>

namespace RE
{
    class cUnit : public MtObject
    {
    public:
        static const MtDTI* DTI() { return REL::Relocation<const MtDTI*>(RE::DTI::cUnit).Get(); }

        // members
        std::uint32_t unk04; // 04
        cUnit* next;         // 08 next unit of the same move line (runtime: walking it from the line's first unit)
        cUnit* prev;         // 0C
        float deltaTime;     // 10 mDeltaTime
        std::uint32_t rno;   // 14 mRno
    };
    static_assert(offsetof(cUnit, next) == 0x08);
    static_assert(offsetof(cUnit, rno) == 0x14);

    class uCoord : public cUnit
    {
    public:
        static const MtDTI* DTI() { return REL::Relocation<const MtDTI*>(RE::DTI::uCoord).Get(); }

        uCoord*& Parent() { return Field<uCoord*>(this, 0x18); }             // mpParent
        std::int32_t& ParentNo() { return Field<std::int32_t>(this, 0x1C); } // mParentNo (joint index)
        MtVector3& Pos() { return Field<MtVector3>(this, 0x30); }            // mPos (world position for characters)
        MtQuaternion& Quat() { return Field<MtQuaternion>(this, 0x40); }     // mQuat
        MtVector3& Scale() { return Field<MtVector3>(this, 0x50); }          // mScale
    };

    class uModel : public uCoord
    {
    public:
        static const MtDTI* DTI() { return REL::Relocation<const MtDTI*>(RE::DTI::uModel).Get(); }

        std::uint32_t& LightGroup() { return Field<std::uint32_t>(this, 0x120); } // mLightGroup
        std::uint32_t& RenderMode() { return Field<std::uint32_t>(this, 0x29C); } // mRenderMode
        std::int32_t& LOD() { return Field<std::int32_t>(this, 0x2B0); }          // mLOD
        float& ShadowAlpha() { return Field<float>(this, 0x308); }                // mShadowAlpha
        bool& SimpleCulling() { return Field<bool>(this, 0x32D); }                // mbSimpleCulling
    };

    // sUnit: 32 move lines (sUnit::MoveLine, 0x20 bytes each from +0x20), every one a linked list of units
    // updated in order. Names read from the lines at runtime ("PL     :06", "EM     :09"...).
    class sUnit : public cSystem
    {
    public:
        enum Line : std::uint32_t
        {
            kPrePro = 0,
            kEvSdl = 1,
            kSdl = 2,
            kSCR = 3,
            kFilter = 4,
            kIntel = 5,  // uPlIntel, uEmIntel*
            kPlayer = 6, // "PL": uPlayer
            kNavi = 7,
            kCamera = 8,  // uCameraCtrl
            kEnemy = 9,   // "EM": uEm*
            kWeapon = 10, // "WEP": uWp*
            kObject = 12, // "OM": uOm*
            kHavok = 13,
            kItemEnemy = 14,
            kItemScene = 15,
            kCloth = 17,
            kFxCore = 18,
            kFxPawn = 19,
            kFxStEnv = 20,
            kFxLight = 21,
            kBnd = 22,
            kLight = 23,
            kSLight = 24,
            kOPT = 25,
            kID1 = 26,
            kID2 = 27,
            kMessage = 28,
            kLight2 = 29,
            kPostPro = 30,
            kLineCount = 32,
        };

        struct MoveLine // sUnit::MoveLine
        {
            void* vtbl;          // 00
            const char* name;    // 04 "PL     :06"
            std::uint32_t flags; // 08
            cUnit* first;        // 0C
            cUnit* last;         // 10
            float speed;         // 14 1.0 (per-line time scale?)
            std::uint32_t index; // 18
            std::uint32_t unk1C; // 1C
        };
        static_assert(sizeof(MoveLine) == 0x20);

        static const MtDTI* DTI() { return REL::Relocation<const MtDTI*>(RE::DTI::sUnit).Get(); }
        static sUnit* GetSingleton() { return *REL::Relocation<sUnit**>(REL::Offset{ 0xE340B0 }); }

        MoveLine& GetLine(Line Index) { return reinterpret_cast<MoveLine*>(reinterpret_cast<std::uintptr_t>(this) + 0x20)[Index]; }

        template <class F>
        void ForEachUnit(Line Index, F&& Visit)
        {
            for (cUnit* Unit = GetLine(Index).first; Unit; Unit = Unit->next)
            {
                if (!Visit(Unit))
                {
                    return;
                }
            }
        }
    };
}
