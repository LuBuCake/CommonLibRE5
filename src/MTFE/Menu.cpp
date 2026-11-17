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

#include "MTFE/Menu.h"

#include "MTFE/API.h"
#include "MTFE/Log.h"

namespace MTFE::Menu
{
    namespace
    {
        const MTFEMenuInterface* MenuInterface = nullptr;
        bool Queried = false;

        // Callbacks live for the whole session (the menu keeps the context pointer).
        void CallInt(MTFEMenuControl, std::int32_t Value, void* Context)
        {
            (*static_cast<std::function<void(std::int32_t)>*>(Context))(Value);
        }

        void CallBool(MTFEMenuControl, std::int32_t Value, void* Context)
        {
            (*static_cast<std::function<void(bool)>*>(Context))(Value != 0);
        }

        void CallVoid(MTFEMenuControl, std::int32_t, void* Context)
        {
            (*static_cast<std::function<void()>*>(Context))();
        }

        Control Add(MTFEMenuControlDesc Description)
        {
            const auto* Api = GetInterface();
            if (!Api)
            {
                return {};
            }
            const Control Result(Api->AddControl(GetPluginHandle(), &Description));
            if (!Result)
            {
                Log::Warn("menu: control '{}' rejected", Description.label ? Description.label : "");
            }
            return Result;
        }

        MTFEMenuControlDesc Desc(std::uint32_t Type, const std::string& Key, const std::string& Label, const std::string& Help)
        {
            MTFEMenuControlDesc Description{};
            Description.size = sizeof(Description);
            Description.type = Type;
            Description.key = Key.empty() ? nullptr : Key.c_str();
            Description.label = Label.c_str();
            Description.help = Help.c_str();
            return Description;
        }
    }

    std::int32_t Control::Get() const
    {
        const auto* Api = GetInterface();
        return Api && _handle ? Api->GetValue(_handle) : 0;
    }

    void Control::Set(std::int32_t Value) const
    {
        if (const auto* Api = GetInterface(); Api && _handle)
        {
            Api->SetValue(_handle, Value);
        }
    }

    const MTFEMenuInterface* GetInterface()
    {
        if (!Queried && GetLoadInterface())
        {
            Queried = true;
            MenuInterface = static_cast<const MTFEMenuInterface*>(GetLoadInterface()->QueryInterface(MTFE_INTERFACE_MENU));
            if (MenuInterface && MenuInterface->interfaceVersion < MTFE_MENU_INTERFACE_VERSION)
            {
                MenuInterface = nullptr;
            }
        }
        return MenuInterface;
    }

    void SetTitle(std::string_view Title)
    {
        if (const auto* Api = GetInterface())
        {
            Api->SetPageTitle(GetPluginHandle(), std::string(Title).c_str());
        }
    }

    Control AddHeader(std::string_view Text)
    {
        const std::string Label(Text);
        return Add(Desc(MTFE_MENU_HEADER, "", Label, ""));
    }

    Control AddToggle(std::string_view Key, std::string_view Label, std::string_view Help, bool DefaultValue, std::function<void(bool)> OnChange)
    {
        const std::string KeyText(Key), LabelText(Label), HelpText(Help);
        auto Description = Desc(MTFE_MENU_TOGGLE, KeyText, LabelText, HelpText);
        Description.defaultValue = DefaultValue ? 1 : 0;
        if (OnChange)
        {
            Description.onChange = CallBool;
            Description.context = new std::function<void(bool)>(std::move(OnChange));
        }
        return Add(Description);
    }

    Control AddList(std::string_view Key, std::string_view Label, std::string_view Help, const std::vector<std::string>& Items,
                    std::int32_t DefaultIndex, std::function<void(std::int32_t)> OnChange)
    {
        const std::string KeyText(Key), LabelText(Label), HelpText(Help);
        std::vector<const char*> Texts;
        for (const auto& Item : Items)
        {
            Texts.push_back(Item.c_str());
        }
        auto Description = Desc(MTFE_MENU_LIST, KeyText, LabelText, HelpText);
        Description.defaultValue = DefaultIndex;
        Description.items = Texts.data();
        Description.itemCount = static_cast<std::uint32_t>(Texts.size());
        if (OnChange)
        {
            Description.onChange = CallInt;
            Description.context = new std::function<void(std::int32_t)>(std::move(OnChange));
        }
        return Add(Description);
    }

    Control AddNumber(std::string_view Key, std::string_view Label, std::string_view Help, std::int32_t Min, std::int32_t Max,
                      std::int32_t Step, std::int32_t DefaultValue, std::uint32_t Decimals, std::function<void(std::int32_t)> OnChange)
    {
        const std::string KeyText(Key), LabelText(Label), HelpText(Help);
        auto Description = Desc(MTFE_MENU_NUMBER, KeyText, LabelText, HelpText);
        Description.minValue = Min;
        Description.maxValue = Max;
        Description.step = Step;
        Description.defaultValue = DefaultValue;
        Description.decimals = Decimals;
        if (OnChange)
        {
            Description.onChange = CallInt;
            Description.context = new std::function<void(std::int32_t)>(std::move(OnChange));
        }
        return Add(Description);
    }

    Control AddHotkey(std::string_view Key, std::string_view Label, std::string_view Help, std::int32_t DefaultKey,
                      std::function<void(std::int32_t)> OnChange)
    {
        const std::string KeyText(Key), LabelText(Label), HelpText(Help);
        auto Description = Desc(MTFE_MENU_HOTKEY, KeyText, LabelText, HelpText);
        Description.defaultValue = DefaultKey;
        if (OnChange)
        {
            Description.onChange = CallInt;
            Description.context = new std::function<void(std::int32_t)>(std::move(OnChange));
        }
        return Add(Description);
    }

    Control AddButton(std::string_view Label, std::string_view Help, std::function<void()> OnPress)
    {
        const std::string LabelText(Label), HelpText(Help);
        auto Description = Desc(MTFE_MENU_BUTTON, "", LabelText, HelpText);
        if (OnPress)
        {
            Description.onChange = CallVoid;
            Description.context = new std::function<void()>(std::move(OnPress));
        }
        return Add(Description);
    }
}
