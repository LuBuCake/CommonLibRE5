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

#include "RE/IDs_DTI.h"
#include "RE/MT/MtObject.h"
#include "RE/Offsets.h"

#include <Windows.h>

namespace RE
{
    // MT Framework system base. The ctor (e.g. sMain 0x425EE0) sets the vftable, then InitializeCriticalSection(+4).
    class cSystem : public MtObject
    {
    public:
        static const MtDTI* DTI() { return REL::Relocation<const MtDTI*>(RE::DTI::cSystem).Get(); }

        // members
        CRITICAL_SECTION lock; // 04
        bool unk1C;            // 1C
    };

    // The application's main system: in RE5 an sBioMain (0x10570 bytes), created by the app init through
    // DTI->NewInstance(); the main loop calls vfunc 6 once per frame. Offsets: sBioMain's own MT properties
    // (live dump, 2026-11-14).
    class sMain : public cSystem
    {
    public:
        static constexpr std::size_t kFrameUpdate = 6;

        static const MtDTI* DTI() { return REL::Relocation<const MtDTI*>(RE::DTI::sMain).Get(); }

        // nullptr until GAME_INIT.
        static sMain* GetSingleton() { return *REL::Relocation<sMain**>(Offset::sMain::Singleton); }

        std::uint64_t& Timer() { return Field<std::uint64_t>(this, 0x20); }         // mTimer
        std::uint32_t& FrameTimer() { return Field<std::uint32_t>(this, 0x30); }    // mFrameTimer
        float& Fps() { return Field<float>(this, 0x38); }                           // mFps
        float& MaxFps() { return Field<float>(this, 0x3C); }                        // mMaxFps (the game's FPS cap)
        float& ActualFps() { return Field<float>(this, 0x44); }                     // mActualFps
        bool& Pause() { return Field<bool>(this, 0x64); }                           // mPause
        float& DeltaTime() { return Field<float>(this, 0x68); }                     // mDeltaTime
        float& DeltaTimeBorder() { return Field<float>(this, 0x6C); }               // mDeltaTimeBorder
        float& DeltaTimeLimit() { return Field<float>(this, 0x70); }                // mDeltaTimeLimite
        float& GlobalSpeed() { return Field<float>(this, 0x74); }                   // mGlobalSpeed (time scale)
        float& DelayFrame() { return Field<float>(this, 0x78); }                    // mDelayFrame
        std::uint32_t& SystemFlag() { return Field<std::uint32_t>(this, 0x10434); } // mSystemFlag
        std::uint32_t& StatusFlag() { return Field<std::uint32_t>(this, 0x10438); } // mStatusFlag
        std::uint32_t& FrameRateMode() { return Field<std::uint32_t>(this, 0x1052C); }
        std::uint32_t& ShadowQuality() { return Field<std::uint32_t>(this, 0x10530); }
        std::uint32_t& LODQuality() { return Field<std::uint32_t>(this, 0x10534); }

        // System pointers kept by sBioMain (mp<Name>), same objects as the singletons in RE/MT/Systems.h.
        enum SystemSlot : std::uintptr_t
        {
            kRender = 0x10340,
            kUnit = 0x10348,
            kResource = 0x1034C,
            kCamera = 0x10350,
            kPad = 0x10354,
            kPrim = 0x10358,
            kCollision = 0x10360,
            kLight = 0x10364,
            kShader = 0x10368,
            kSound = 0x1036C,
            kCapture = 0x10370,
            kArea = 0x10374,
            kGpuParticle = 0x10378,
            kEffectVFR = 0x1037C,
            kAI = 0x10384,
            kMouse = 0x10388,
            kKeyboard = 0x1038C,
            kCursor = 0x10390,
            kBioVibration = 0x10394,
            kBenchmark = 0x10398,
            kFps = 0x1039C,
            kBioSound = 0x103A0,
            kBioSoundMan = 0x103A4,
            kLayoutSpr = 0x103B0,
            kEventPlayer = 0x103B4,
            kAreaHit = 0x103B8,
            kActionButton = 0x103BC,
            kScenario = 0x103C0,
            kParameter = 0x103C4,
            kPlayer = 0x103C8,
            kEnemy = 0x103CC,
            kObjModel = 0x103D0,
            kItem = 0x103D4,
            kShell = 0x103D8,
            kCubeMapInterp = 0x103DC,
            kLightLinker = 0x103E0,
            kHavok = 0x103E4,
            kNetwork = 0x103E8,
            kStorage = 0x103EC,
            kLoadMan = 0x103F0,
            kInfoMan = 0x103F4,
            kId = 0x103F8,
            kGameSys = 0x1042C, // mpGameSys (points into .text at the title menu; an object in game)
        };

        MtObject* GetSystem(SystemSlot Slot) { return Field<MtObject*>(this, Slot); }
    };
}
