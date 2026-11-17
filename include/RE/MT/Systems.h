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

// MT Framework systems (cSystem singletons). Each GetSingleton() reads the static instance pointer found by
// runtime_scan.py on the Steam 2023 runtime (2026-11-14: every global pointed to an object of that class).
// Game systems keep it 4 bytes before their DTI; framework systems in a block at 0x12337DC..0x1249C78.
// The objects are RE5's subclasses (sBioCamera for sCamera, ...); the comment names the real class.
// nullptr before GAME_INIT. Fields: only those confirmed through MT properties or code.

#include "RE/IDs_DTI.h"
#include "RE/MT/sMain.h"

namespace RE
{
    template <class T>
    T* ReadSingleton(std::uintptr_t Rva)
    {
        return *REL::Relocation<T**>(REL::Offset{ Rva });
    }

#define RE5_SYSTEM(Name, DtiName, GlobalRva)                                                        \
    class Name : public cSystem                                                                     \
    {                                                                                               \
    public:                                                                                         \
        static const MtDTI* DTI() { return REL::Relocation<const MtDTI*>(RE::DTI::DtiName).Get(); } \
        static Name* GetSingleton() { return ReadSingleton<Name>(GlobalRva); }                      \
    };

    // framework
    RE5_SYSTEM(sApp, sApp, 0xE337DC)
    // sUnit: RE/MT/cUnit.h (move lines), global 0xE340B0
    RE5_SYSTEM(sShader, sShader, 0xE3452C) // sBioShader
    RE5_SYSTEM(sArea, sArea, 0xE34578)     // sBioArea
    RE5_SYSTEM(sCamera, sCamera, 0xE3457C) // sBioCamera
    RE5_SYSTEM(sResource, sResource, 0xE34580)
    RE5_SYSTEM(sBenchmark, sBenchmark, 0xE34584) // sBioBenchmark
    RE5_SYSTEM(sRender, sRender, 0xE345D4)       // sBioRender
    RE5_SYSTEM(sLight, sLight, 0xE345D8)         // sBioLight
    RE5_SYSTEM(sToolBase, sToolBase, 0xE345EC)   // sBioTool
    RE5_SYSTEM(sCapture, sCapture, 0xE34698)
    RE5_SYSTEM(sHavok, sHavok, 0xE0C744)     // sBioHavok
    RE5_SYSTEM(sNetwork, sNetwork, 0xE49C1C) // sBioNetwork
    RE5_SYSTEM(sNetworkLibraryWindows, sNetworkLibraryWindows, 0xE49C20)
    // sPad (sBioPad): RE/Game/Input.h, global 0xE49C40
    RE5_SYSTEM(sVibration, sVibration, 0xE49C44)   // sBioVibration
    RE5_SYSTEM(sEffectBase, sEffectBase, 0xE49C5C) // sBioEffect
    RE5_SYSTEM(sSoundN, sSoundN, 0xE49C60)
    RE5_SYSTEM(sCollision, sCollision, 0xE49C64) // sBioCollision
    RE5_SYSTEM(sPrim, sPrim, 0xE49C68)
    RE5_SYSTEM(sGpuParticle, sGpuParticle, 0xE49C70)
    RE5_SYSTEM(sKeyboard, sKeyboard, 0xE49C74)
    RE5_SYSTEM(sMouse, sMouse, 0xE49C78)

    // game (instance pointer right before the DTI)
    // sActionButton: RE/Game/Input.h (logical pads), global 0xDB1794
    RE5_SYSTEM(sAI, sAI, 0xDB1894)
    RE5_SYSTEM(sAreaHit, sAreaHit, 0xDB18E0)
    RE5_SYSTEM(sBioSound, sBioSound, 0xDB1D0C)
    RE5_SYSTEM(sBioSoundMan, sBioSoundMan, 0xDB1D94)
    RE5_SYSTEM(sStorage, sStorage, 0xDB1DD8) // sBioStorage
    RE5_SYSTEM(sCubeMapInterp, sCubeMapInterp, 0xDB1E4C)
    RE5_SYSTEM(sCursor, sCursor, 0xDB1FA4)
    RE5_SYSTEM(sEnemy, sEnemy, 0xDB1FCC)
    RE5_SYSTEM(sEventPlayer, sEventPlayer, 0xDB2018)
    RE5_SYSTEM(sFps, sFps, 0xDB209C)
    RE5_SYSTEM(sId, sId, 0xDB20C4)
    // sInfoMan: RE/Game/GameInfo.h (save/game state), global 0xDB2158
    RE5_SYSTEM(sItem, sItem, 0xDB2200)
    RE5_SYSTEM(sLayoutSpr, sLayoutSpr, 0xDB2334)
    RE5_SYSTEM(sLightLinker, sLightLinker, 0xDB2384)
    RE5_SYSTEM(sLoadMan, sLoadMan, 0xDB26C4)
    RE5_SYSTEM(sMessage, sMessage, 0xDB26F0)
    RE5_SYSTEM(sObjModel, sObjModel, 0xDB2720)
    RE5_SYSTEM(sParameter, sParameter, 0xDB27B4)
    // sPlayer: RE/Game/Character.h (players), global 0xDB27DC
    RE5_SYSTEM(sScenario, sScenario, 0xDE8660)
    RE5_SYSTEM(sShell, sShell, 0xDE86AC)
    RE5_SYSTEM(sTransition, sTransition, 0xDE8804)

#undef RE5_SYSTEM
}
