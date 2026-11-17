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

// Input layers (Steam 2023). All fields: [prop] MT properties; button meanings: [rt] keyboard/mouse trace in a
// Mercenaries match (2026-11-14, tools action_trace.py).
//
//   sPad::Pad   raw device pad (XInput). The keyboard does NOT set its buttons (On stayed 0 while playing on keyboard).
//   cPad        logical pad, one per player in sActionButton: keyboard/mouse and controller end up here as the same
//               buttons. Use this one.
//   cPadEx      the player's action layer (owned by the player's/AI's "think" object): mOn[512] one bool per game action,
//               aim/recoil angles. The partner's lives in cPlIntelNpcThink + 0x880.

#include "RE/IDs_DTI.h"
#include "RE/MT/cUnit.h"
#include "RE/MT/sMain.h"

namespace RE
{
    // Raw device pad (sPad::Pad, 0x2C0).
    struct RawPad
    {
        void* vtbl;                 // 000
        std::uint8_t beFlag;        // 004
        std::uint8_t rno;           // 005
        std::int8_t padNo;          // 006
        std::uint8_t kind;          // 007 [rt] 3 = the active pad
        std::uint32_t ability;      // 008
        std::uint32_t inputAttr;    // 00C
        std::uint32_t socketNo;     // 010
        std::uint8_t pad014[0x148]; // 014
        std::uint32_t on;           // 15C
        std::uint32_t old;          // 160
        std::uint32_t trg;          // 164 pressed this frame
        std::uint32_t rel;          // 168 released this frame
        std::uint32_t chg;          // 16C
        std::uint32_t rep;          // 170
        std::int32_t rx, ry;        // 174
        std::int32_t lx, ly;        // 17C
        std::uint8_t rt;            // 184
        std::uint8_t lt;            // 185
        std::uint8_t pad186[0x13A]; // 186
    };
    static_assert(sizeof(RawPad) == 0x2C0);
    static_assert(offsetof(RawPad, on) == 0x15C);

    // Logical pad (cPad, 0x220).
    class cPad : public MtObject
    {
    public:
        enum Button : std::uint32_t // [rt] keyboard/mouse trace; movement is analog (no bit)
        {
            kKnife = 1u << 10,
            kAim = 1u << 11,
            kInventory = 1u << 12,
            kReload = 1u << 13,
            kRun = 1u << 14,
            kFire = 1u << 15,
        };

        bool Held(std::uint32_t Buttons) const { return (on & Buttons) != 0; }
        bool Pressed(std::uint32_t Buttons) const { return (trg & Buttons) != 0; }
        bool Released(std::uint32_t Buttons) const { return (rel & Buttons) != 0; }

        // members
        bool enable;                // 004
        bool enableOld;             // 005
        std::uint8_t pad006[0x2E];  // 006
        std::uint32_t on;           // 034
        std::uint32_t old;          // 038
        std::uint32_t trg;          // 03C
        std::uint32_t rel;          // 040
        std::uint8_t pad044[0x188]; // 044
        float stickDirL;            // 1CC
        float stickDirR;            // 1D0
        float stickPowL;            // 1D4
        float stickPowR;            // 1D8
        float triggerL;             // 1DC
        float triggerR;             // 1E0
        std::uint8_t pad1E4[0x14];  // 1E4
        std::int32_t mousePos[2];   // 1F8 mMousePos (point)
        float mouseDir;             // 200
        float mousePow;             // 204
        std::uint8_t pad208[4];     // 208
        float mouseAxisX;           // 20C
        float mouseAxisY;           // 210
        float mouseAxisZ;           // 214
        std::uint8_t pad218[8];     // 218
    };
    static_assert(sizeof(cPad) == 0x220);
    static_assert(offsetof(cPad, on) == 0x34);
    static_assert(offsetof(cPad, stickDirL) == 0x1CC);
    static_assert(offsetof(cPad, mouseAxisZ) == 0x214);

    // Player action layer (cPadEx, 0x730).
    class cPadEx : public MtObject
    {
    public:
        static constexpr std::size_t kActions = 0x200;

        // Action indices seen in the trace (mOn), names ours.
        enum Action : std::uint32_t
        {
            kIdle0 = 0,
            kMoveForward = 2,
            kMoveLeft = 3,
            kMoveRight = 4,
            kRunning = 5,
            kMoveBack = 6,
            kQuickTurn = 9,
            kAiming = 22,
            kFiring = 30,
            kKnifeReady = 34,
            kReloading = 169,
        };

        bool IsOn(std::uint32_t Index) const { return Index < kActions && on[Index]; }
        bool WasOn(std::uint32_t Index) const { return Index < kActions && old[Index]; }

        // members
        bool on[kActions];                // 004 mOn
        bool old[kActions];               // 204 mOld
        bool padStop;                     // 404 mbPadStop
        std::uint8_t pad405[3];           // 405
        float camAng[2];                  // 408
        MtVector3 stickVecL;              // 410
        MtVector3 stickVecR;              // 420
        std::uint32_t requestEquipSlotNo; // 430
        std::uint8_t pad434[0xC];         // 434
        std::uint32_t emotionNoRequest;   // 440
        float walkAngleY;                 // 444
        std::uint8_t pad448[8];           // 448
        float waistAngleY;                // 450
        float aimAngleY;                  // 454
        float aimRateX;                   // 458
        float aimRateZoom;                // 45C
        std::uint8_t pad460[0xC];         // 460
        float accelRate;                  // 46C
        float breakRate;                  // 470
        float steeringRate;               // 474
        std::uint8_t pad478[0x44];        // 478
        float recoilAng;                  // 4BC
        float swingAng;                   // 4C0
        float recoilCorrectAng;           // 4C4
        float swingCorrectAng;            // 4C8
        std::uint8_t pad4CC[4];           // 4CC
        float scopeMousePowRateX;         // 4D0
        float scopeMousePowRateY;         // 4D4
        float pointingAimSpd;             // 4D8
        std::uint8_t pad4DC[0xC];         // 4DC
        cPad pad;                         // 4E8 mPad (embedded)
        std::uint8_t pad708[0x28];        // 708
    };
    static_assert(offsetof(cPadEx, padStop) == 0x404);
    static_assert(offsetof(cPadEx, recoilAng) == 0x4BC);
    static_assert(offsetof(cPadEx, pad) == 0x4E8);
    static_assert(sizeof(cPadEx) == 0x730);

    class sPad : public cSystem
    {
    public:
        static const MtDTI* DTI() { return REL::Relocation<const MtDTI*>(RE::DTI::sPad).Get(); }
        static sPad* GetSingleton() { return *REL::Relocation<sPad**>(REL::Offset{ 0xE49C40 }); } // sBioPad

        RawPad& GetPad(std::size_t Index) { return Field<RawPad>(this, 0x34 + 0x2C0 * Index); } // [prop] mPad[2]
        bool& Active() { return Field<bool>(this, 0x20); }
        bool& XInput() { return Field<bool>(this, 0x5BC); }
        std::int16_t& KeyboardPlayerNo() { return Field<std::int16_t>(this, 0x614); } // sBioPad
    };

    // Game key codes: keyboard 1..0x61, mouse 0x62..0x66 (0 = none). The values of the KEYBOARD/MOUSE options and of
    // MTFE::Menu::AddHotkey. [code] table 0x10E5870 (16 bytes: code, ?, ?, id of the message with the key name);
    // table 0x10E5EF0 = Windows virtual key of each keyboard code (1 = VK_BACK, 0x0B = VK_ESCAPE, 0x21 = 'A',
    // 0x4A = VK_F1 .. 0x51 = VK_F8); for mouse codes it holds the button index (0, 1, 2) / wheel (8, 9).
    namespace GameKey
    {
        inline constexpr std::int32_t kF1 = 0x4A, kF8 = 0x51;

        inline std::uint32_t ToVirtualKey(std::int32_t Code) // 0 for none / mouse codes
        {
            if (Code <= 0 || Code > 0x61)
            {
                return 0;
            }
            return REL::Relocation<const std::uint32_t*>(REL::Offset{ 0xCE5EF0 }).Get()[Code];
        }

        inline bool IsDown(std::int32_t Code) // [code] 0x799FB0 sPad::IsKeyDown (false while the game window is inactive)
        {
            using Fn = bool(__thiscall*)(void*, std::int32_t);
            auto* Pad = sPad::GetSingleton();
            return Code > 0 && Code <= 0x66 && Pad && reinterpret_cast<Fn>(REL::Offset{ 0x399FB0 }.GetAddress())(Pad, Code);
        }

        // Edge detection for one key: true on the frame the key goes down. Keep one per hotkey, poll every frame.
        class Watch
        {
        public:
            bool Pressed(std::int32_t Code)
            {
                const bool Down = IsDown(Code);
                const bool JustPressed = Down && !_down;
                _down = Down;
                return JustPressed;
            }

        private:
            bool _down = false;
        };
    }

    class sActionButton : public cSystem
    {
    public:
        static const MtDTI* DTI() { return REL::Relocation<const MtDTI*>(RE::DTI::sActionButton).Get(); }
        static sActionButton* GetSingleton() { return *REL::Relocation<sActionButton**>(REL::Offset{ 0xDB1794 }); }

        // [rt] 4 cPad embedded (vtable found at +0x50 + 0x2D0*i); pad 0 followed the keyboard in game.
        cPad& GetPad(std::size_t Player) { return Field<cPad>(this, 0x50 + 0x2D0 * Player); }
    };
}
