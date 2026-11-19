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

#include "MTFE/PluginAPI.h"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

// Mod settings page in the game's Extras > Special Settings > MODS (MTFE_INTERFACE_MENU). One page per plugin; rows appear in the order
// they are added. Values with a key are saved in <game>\MTFE\Config\<plugin>.ini [Settings] and are already loaded
// when Add* returns. Texts: ASCII only (the menu font has no accents).
//
//   auto Enabled = MTFE::Menu::AddToggle("bEnabled", "Enabled", "Turns the mod on and off.", true);
//   auto Key = MTFE::Menu::AddHotkey("iKey", "Hotkey", "Key that does the thing.", 0x3B);
//   if (Enabled.Get() && RE::GameKey::IsDown(Key.Get())) { ... }
namespace MTFE::Menu
{
    class Control
    {
    public:
        Control() = default;
        explicit Control(MTFEMenuControl Value) : _handle(Value) {}

        explicit operator bool() const { return _handle != MTFE_INVALID_MENU_CONTROL; }
        MTFEMenuControl Handle() const { return _handle; }

        std::int32_t Get() const;
        bool GetBool() const { return Get() != 0; }
        void Set(std::int32_t Value) const; // clamps, saves, calls onChange

    private:
        MTFEMenuControl _handle = MTFE_INVALID_MENU_CONTROL;
    };

    const MTFEMenuInterface* GetInterface(); // nullptr: the loader has no menu (old MTFE)

    void SetTitle(std::string_view Title); // default: the plugin name

    Control AddHeader(std::string_view Text);
    Control AddToggle(std::string_view Key, std::string_view Label, std::string_view Help, bool DefaultValue,
                      std::function<void(bool)> OnChange = {});
    Control AddList(std::string_view Key, std::string_view Label, std::string_view Help, const std::vector<std::string>& Items,
                    std::int32_t DefaultIndex, std::function<void(std::int32_t)> OnChange = {});
    // Shown as value / 10^decimals: AddNumber("fScale", "Scale", "", 5, 50, 1, 10, 1) edits 0.5 .. 5.0 (stored as 5 .. 50).
    Control AddNumber(std::string_view Key, std::string_view Label, std::string_view Help, std::int32_t Min, std::int32_t Max,
                      std::int32_t Step, std::int32_t DefaultValue, std::uint32_t Decimals = 0, std::function<void(std::int32_t)> OnChange = {});
    // Value = game key code (RE::GameKey), 0 = none. The player sets it by confirming the row and pressing a key.
    Control AddHotkey(std::string_view Key, std::string_view Label, std::string_view Help, std::int32_t DefaultKey,
                      std::function<void(std::int32_t)> OnChange = {});
    Control AddButton(std::string_view Label, std::string_view Help, std::function<void()> OnPress);
}
