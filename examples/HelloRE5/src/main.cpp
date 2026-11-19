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

// HelloRE5: smoke test of MTFE + CommonLibRE5. Logs to <game>\MTFE\Logs\HelloRE5.log:
//   every loader message, the MtDTI tree at PRE_WINMAIN, sMain at GAME_INIT, and a frame counter.
// Settings page in Extras > Special Settings > MODS > Hello RE5 (one row of every kind of control), saved in MTFE\Config\HelloRE5.ini.

#include <RE/RE5.h>
#include <MTFE/MTFE.h>

#include <Windows.h>

#include <algorithm>

namespace
{
    std::uint32_t Frames = 0;
    DWORD LastReport = 0;

    MTFE::Menu::Control HealEnabled, HealKey, HealPercent, ReportEvery;

    void DumpDTI()
    {
        std::uint32_t Count = 0;
        const auto CountClass = [&](const RE::MtDTI*)
        {
            ++Count;
            return true;
        };
        RE::MtDTI::ForEach(CountClass);
        MTFE::Log::Info("MtDTI tree: {} classes", Count);

        for (const char* Name : { "uPlayer", "uEnemy", "uCamera", "sMain" })
        {
            if (const auto* Dti = RE::MtDTI::Find(Name))
            {
                MTFE::Log::Info("  {}: size 0x{:X}, parent {}, id {:08X}", Dti->GetName(), Dti->GetSize(),
                                Dti->GetParent() && Dti->GetParent()->GetName() ? Dti->GetParent()->GetName() : "-", Dti->GetID());
            }
            else
            {
                MTFE::Log::Warn("  {}: not found", Name);
            }
        }

        const auto* Player = REL::Relocation<const RE::MtDTI*>(RE::DTI::uPlayer).Get();
        MTFE::Log::Info("RE::DTI::uPlayer -> {} (same as Find: {})", Player->GetName(), Player == RE::MtDTI::Find("uPlayer"));
    }

    template <class T>
    void CheckSystem(const char* Name, int& Ok, int& Bad)
    {
        auto* System = T::GetSingleton();
        const auto* Dti = System ? System->GetDTI() : nullptr;
        const bool Good = System && System->IsKindOf(T::DTI());
        (Good ? Ok : Bad)++;
        if (!Good)
        {
            MTFE::Log::Warn("  {}: {:08X} ({})", Name, reinterpret_cast<std::uintptr_t>(System), Dti ? Dti->GetName() : "null");
        }
    }

    void CheckSystems()
    {
        int Ok = 0, Bad = 0;
#define CHECK(T) CheckSystem<RE::T>(#T, Ok, Bad)
        CHECK(sApp);
        CHECK(sUnit);
        CHECK(sShader);
        CHECK(sArea);
        CHECK(sCamera);
        CHECK(sResource);
        CHECK(sBenchmark);
        CHECK(sRender);
        CHECK(sLight);
        CHECK(sToolBase);
        CHECK(sCapture);
        CHECK(sHavok);
        CHECK(sNetwork);
        CHECK(sNetworkLibraryWindows);
        CHECK(sPad);
        CHECK(sVibration);
        CHECK(sEffectBase);
        CHECK(sSoundN);
        CHECK(sCollision);
        CHECK(sPrim);
        CHECK(sGpuParticle);
        CHECK(sKeyboard);
        CHECK(sMouse);
        CHECK(sActionButton);
        CHECK(sAI);
        CHECK(sAreaHit);
        CHECK(sBioSound);
        CHECK(sBioSoundMan);
        CHECK(sStorage);
        CHECK(sCubeMapInterp);
        CHECK(sCursor);
        CHECK(sEnemy);
        CHECK(sEventPlayer);
        CHECK(sFps);
        CHECK(sId);
        CHECK(sInfoMan);
        CHECK(sItem);
        CHECK(sLayoutSpr);
        CHECK(sLightLinker);
        CHECK(sLoadMan);
        CHECK(sMessage);
        CHECK(sObjModel);
        CHECK(sParameter);
        CHECK(sPlayer);
        CHECK(sScenario);
        CHECK(sShell);
        CHECK(sTransition);
#undef CHECK
        auto* Main = RE::sMain::GetSingleton();
        const bool Same = Main->GetSystem(RE::sMain::kPlayer) == RE::sPlayer::GetSingleton() &&
                          Main->GetSystem(RE::sMain::kCamera) == RE::sCamera::GetSingleton() &&
                          Main->GetSystem(RE::sMain::kItem) == RE::sItem::GetSingleton();
        MTFE::Log::Info("systems: {} ok, {} wrong; sMain pointers == singletons: {}; maxFps {:.0f}, globalSpeed {:.2f}",
                        Ok, Bad, Same, Main->MaxFps(), Main->GlobalSpeed());

        auto* Camera = RE::sCamera::GetSingleton();
        RE::MtPropertyList Properties;
        Properties.Collect(Camera);
        std::size_t PropertyCount = 0;
        Properties.ForEach([&](const RE::MtProperty&)
                           { ++PropertyCount; });
        const auto* Blank = Properties.Find("mBlankColor");
        MTFE::Log::Info("sCamera ({}): {} properties, mBlankColor at +0x{:X}", Camera->GetDTI()->GetName(), PropertyCount,
                        Blank && Blank->IsField() ? Blank->FieldOffset() : 0);
    }

    void ReportMatch()
    {
        auto* Players = RE::sPlayer::GetSingleton();
        auto* Player = Players ? Players->GetPlayer(0) : nullptr;
        if (!Player)
        {
            return;
        }
        const auto& Pos = Player->Pos();
        MTFE::Log::Info("P1 {} hp {}/{} pos ({:.0f}, {:.0f}, {:.0f}) speed {:.2f} partner {:08X} (players: {})",
                        Player->GetDTI()->GetName(), Player->Hp(), Player->HpMax(), Pos.x, Pos.y, Pos.z, Player->MoveSpeed(),
                        reinterpret_cast<std::uintptr_t>(Player->Partner()), Players->GetPlayerCount());

        // Generated ids (RE/IDs_Fields.h), all found from uPlayer through its ancestors: fields (uCoord's mPos, uCharacter's
        // Hp from data/Fields.tsv) and properties read through the game's accessors (cUnit's Name, uCoord's mAngle, and
        // uModel's mTransparency, whose accessors run on the uModel::RenderInfo at +0x298: the id carries that offset).
        const auto& FieldPos = Player->Get(RE::FIELD::uPlayer::mPos);
        const char* UnitName = Player->Get(RE::PROP::uPlayer::Name);
        const auto Angle = Player->Get(RE::PROP::uPlayer::mAngle);
        const float Transparency = Player->Get(RE::PROP::uPlayer::mTransparency);
        const bool Written = Player->Set(RE::PROP::uPlayer::mTransparency, Transparency); // same value back: no change
        MTFE::Log::Info("  ids: mPos field == Pos(): {}, Hp field {}, Name \"{}\", mAngle ({:.2f}, {:.2f}, {:.2f}), mTransparency {:.2f} (set: {})",
                        &FieldPos == &Player->Pos(), Player->Get(RE::FIELD::uPlayer::Hp), UnitName ? UnitName : "", Angle.x, Angle.y,
                        Angle.z, Transparency, Written);
        for (std::size_t i = 0; i < RE::uPlayer::kItemCount; ++i)
        {
            auto& Item = Player->Item(i);
            if (Item.id)
            {
                const auto* Name = RE::Items::GetName(Item.id);
                MTFE::Log::Info("  slot {:2} id 0x{:03X} {:24} {}/{} power {} infinite {} weapon {}", i, Item.id, Name ? Name : "?",
                                Item.num, Item.max, Item.lvPower, Item.IsInfiniteAmmo(),
                                Item.weapon ? Item.weapon->GetDTI()->GetName() : "-");
            }
        }
        if (auto* Equipped = RE::GetInventory(Player)->GetEquipped())
        {
            const auto* Info = RE::Items::GetWeaponInfo(Equipped->id);
            MTFE::Log::Info("  equipped: 0x{:03X} {} (WEAPON_INFO {}, ammo item 0x{:X})", Equipped->id,
                            RE::Items::GetName(Equipped->id) ? RE::Items::GetName(Equipped->id) : "?", Info ? Info->debugName : "-",
                            Info ? Info->bulletId : 0);
        }

        int Enemies = 0, Alive = 0;
        const auto CountEnemy = [&](RE::cUnit* Unit)
        {
            if (auto* Enemy = Unit->As<RE::uEnemy>())
            {
                ++Enemies;
                Alive += Enemy->IsDead() ? 0 : 1;
            }
            return true;
        };
        RE::sUnit::GetSingleton()->ForEachUnit(RE::sUnit::kEnemy, CountEnemy);
        MTFE::Log::Info("  enemies on line EM: {} ({} alive)", Enemies, Alive);

        if (auto* Info = RE::sInfoMan::GetSingleton())
        {
            auto* Game = Info->GetActiveGame();
            const auto& Record = Game->PlayerRecords()[0];
            const auto* Chosen = RE::FindCostume(Record.character, Record.costume);
            const auto* Live = RE::FindCostumeByClass(Player->GetDTI()->GetName());
            MTFE::Log::Info("  game: chapter {} stage {} difficulty {}; P1 record {}/{} = {} ({}), live class matches: {}",
                            Game->ChapterNo(), Game->StageNo(), Game->Difficulty(), static_cast<int>(Record.character), Record.costume,
                            Chosen ? Chosen->PlayerClass : "?", Chosen ? Chosen->Name : "?", Chosen && Chosen == Live);
        }

        if (auto* GameSys = RE::cGameSys::GetSingleton())
        {
            MTFE::Log::Info("  cGameSys: mode {}, mercenaries time {:.1f}, score {}, local mask {}", GameSys->GameMode(),
                            GameSys->MercenariesTime(), GameSys->Score(), GameSys->LocalPlayerMask());
        }
    }

    void OnFrame()
    {
        ++Frames;
        const auto Now = GetTickCount();
        constexpr DWORD kEvery[] = { 5000, 10000, 30000, 0 };
        const DWORD Every = kEvery[std::clamp(ReportEvery.Get(), 0, 3)];
        if (Every && Now - LastReport >= Every)
        {
            MTFE::Log::Info("{} frames in the last {:.1f} s", Frames, (Now - LastReport) / 1000.0);
            ReportMatch();
            Frames = 0;
            LastReport = Now;
        }

        if (auto* Buttons = RE::sActionButton::GetSingleton())
        {
            const auto& Pad = Buttons->GetPad(0);
            if (Pad.Pressed(RE::cPad::kReload))
            {
                MTFE::Log::Info("input: reload pressed (cPad On {:08X})", Pad.on);
            }
            if (Pad.Pressed(RE::cPad::kKnife))
            {
                MTFE::Log::Info("input: knife pressed (cPad On {:08X})", Pad.on);
            }
        }

        static RE::GameKey::Watch HealWatch;
        if (HealWatch.Pressed(HealKey.Get()) && HealEnabled.GetBool())
        {
            // Game methods on the objects: SetHp is the uPawn vfunc 67 (RE::VFUNC::uPawn::SetHp), so uPlayer's override runs.
            if (auto* Player = RE::sPlayer::GetLocalPlayer())
            {
                const auto Before = Player->Hp();
                const int Target = Player->HpMax() * HealPercent.Get() / 100;
                Player->SetHp(static_cast<std::int16_t>(std::max<int>(Before, Target)));
                MTFE::Log::Info("heal key: P1 {} -> {}", Before, Player->Hp());
            }
        }
    }

    // Extras > Special Settings > MODS > Hello RE5: one row of every kind of control.
    void BuildMenu()
    {
        using namespace MTFE::Menu;
        SetTitle("Hello RE5");
        AddHeader("Heal");
        HealEnabled = AddToggle("bHealKey", "Heal key enabled", "Turns the heal key on and off.", true,
                                [](bool On)
                                { MTFE::Log::Info("menu: heal key {}", On ? "on" : "off"); });
        HealKey = AddHotkey("iHealKey", "Heal key", "Key that heals player 1. Confirm, then press the new key.", RE::GameKey::kF8,
                            [](std::int32_t Code)
                            { MTFE::Log::Info("menu: heal key = game code {:X} (VK {:X})", Code, RE::GameKey::ToVirtualKey(Code)); });
        HealPercent = AddNumber("iHealPercent", "Heal to (%)", "Health after the key, in percent of the maximum.", 10, 100, 10, 100);
        AddHeader("Log");
        ReportEvery = AddList("iReportEvery", "Report every", "How often the match report goes to HelloRE5.log.",
                              { "5 seconds", "10 seconds", "30 seconds", "Never" }, 1, [](std::int32_t Index)
                              { MTFE::Log::Info("menu: report every = {}", Index); });
        AddNumber("iScale", "Example decimal", "A number with one decimal: stored as tenths in the INI.", 5, 50, 1, 10, 1);
        const auto WriteReport = []
        {
            MTFE::Log::Info("menu: button pressed");
            ReportMatch();
        };
        AddButton("Write report now", "Writes the match report to HelloRE5.log right away.", WriteReport);
        AddToggle("bExtra1", "Scrolling test 1", "Extra rows to test the scroll.", false);
        AddToggle("bExtra2", "Scrolling test 2", "Extra rows to test the scroll.", true);
    }

    void OnMessage(MTFE::Message* Msg)
    {
        switch (static_cast<MTFE::MessageType>(Msg->type))
        {
        case MTFE::MessageType::kPostLoad:
            MTFE::Log::Info("POST_LOAD");
            break;
        case MTFE::MessageType::kPostPostLoad:
            MTFE::Log::Info("POST_POST_LOAD");
            break;
        case MTFE::MessageType::kPreWinMain:
            MTFE::Log::Info("PRE_WINMAIN");
            DumpDTI();
            break;
        case MTFE::MessageType::kGameInit:
        {
            auto* Main = RE::sMain::GetSingleton();
            const auto* Dti = Main ? Main->GetDTI() : nullptr;
            MTFE::Log::Info("GAME_INIT: sMain {:08X} is a {} (kind of sMain: {})", reinterpret_cast<std::uintptr_t>(Main),
                            Dti ? Dti->GetName() : "?", Main && Main->IsKindOf(RE::sMain::DTI()));
            CheckSystems();
            LastReport = GetTickCount();
            MTFE::AddFrameCallback(OnFrame);
            MTFE::AddTask([]
                          { MTFE::Log::Info("one-shot task ran on the main thread {}", GetCurrentThreadId()); });
        }
        break;
        }
    }
}

MTFE_PLUGIN_LOAD(const MTFE::LoadInterface* Interface)
{
    MTFE::Init(Interface);
    BuildMenu();
    MTFE::Log::Info("HelloRE5 loaded: MTFE {}.{}.{}, runtime {:08X}, supported by CommonLibRE5: {}",
                    MTFE_VERSION_MAJOR(Interface->mtfeVersion), MTFE_VERSION_MINOR(Interface->mtfeVersion),
                    MTFE_VERSION_PATCH(Interface->mtfeVersion), Interface->runtimeVersion, REL::Module::Get().IsSupported());
    return MTFE::RegisterListener(OnMessage);
}
