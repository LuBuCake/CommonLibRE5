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

// PropertyDump (developer tool, not for players). Writes MT properties (name, type, offset) with RE::MtPropertyList.
//   GAME_INIT: every class's createProperty on a zeroed fake object -> <MTFE>\Logs\properties.tsv
//              (accessor-only or crashing classes are partial: their real members are null).
//   live:      each line "<hex address>" in <MTFE>\propdump.txt is a live object, dumped on itself
//              -> <MTFE>\Logs\properties_live.tsv (complete). Polled every 0.5 s; the file is deleted after.

#include <RE/IDs_Classes.h>
#include <RE/RE5.h>
#include <MTFE/MTFE.h>

#include <Windows.h>

#include <cstdio>
#include <cstdlib>
#include <share.h>
#include <string>
#include <vector>

namespace
{
    const char* SafeName(const char* Text)
    {
        __try
        {
            return Text && !IsBadStringPtrA(Text, 128) ? Text : "?";
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return "?";
        }
    }

    bool IsMtObject(std::uintptr_t Address)
    {
        __try
        {
            if (!Address || IsBadReadPtr(reinterpret_cast<void*>(Address), 4))
            {
                return false;
            }
            const auto VTable = *reinterpret_cast<const std::uintptr_t*>(Address);
            return !IsBadReadPtr(reinterpret_cast<void*>(VTable), 0x14);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    void Write(FILE* File, const char* Prefix, const RE::MtPropertyList& List, std::uintptr_t Object, std::size_t Size)
    {
        const auto WriteProperty = [&](const RE::MtProperty& Property)
        {
            char Where[48];
            if (Property.IsField() && Property.get >= Object && Property.get < Object + Size)
            {
                std::snprintf(Where, sizeof(Where), "+0x%X", static_cast<unsigned>(Property.get - Object));
            }
            else if (Property.get)
            {
                std::snprintf(Where, sizeof(Where), "%s0x%08X", (Property.attr & RE::MtProperty::kGetSet) ? "get " : "@", static_cast<unsigned>(Property.get));
            }
            else
            {
                Where[0] = 0;
            }
            std::fprintf(File, "%s\t%s\t%u\t0x%X\t%s\t%u\t%08X\n", Prefix, SafeName(Property.name), static_cast<unsigned>(Property.type), Property.attr,
                         Where, Property.count, static_cast<unsigned>(Property.set));
        };
        List.ForEach(WriteProperty);
    }

    void DumpAll()
    {
        const auto Path = MTFE::GetMTFEPath() + "Logs\\properties.tsv";
        FILE* File = _fsopen(Path.c_str(), "w", _SH_DENYWR);
        if (!File)
        {
            return;
        }
        std::fprintf(File, "class\tname\ttype\tattr\toffset\tcount\tsetter\n");

        const auto Base = REL::Module::Get().GetBase();
        std::size_t PropertyCount = 0, Crashed = 0, Classes = 0;
        for (const auto& Entry : RE::kClasses)
        {
            if (!Entry.VTable)
            {
                continue;
            }
            const auto* Dti = reinterpret_cast<const RE::MtDTI*>(Base + Entry.DTI);
            const std::size_t Size = (Dti->GetSize() + 0x1000 + 0xFFF) & ~std::size_t(0xFFF);
            auto* Object = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, Size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
            if (!Object)
            {
                continue;
            }
            *reinterpret_cast<std::uintptr_t*>(Object) = Base + Entry.VTable;
            {
                RE::MtPropertyList List;
                if (!List.Collect(reinterpret_cast<RE::MtObject*>(Object)))
                {
                    ++Crashed;
                    std::fprintf(File, "%s\t#partial (createProperty crashed on the fake object)\t\t\t\t\t\n", Entry.Name);
                }
                Write(File, Entry.Name, List, reinterpret_cast<std::uintptr_t>(Object), Size);
                List.ForEach([&](const RE::MtProperty&)
                             { ++PropertyCount; });
            }
            VirtualFree(Object, 0, MEM_RELEASE);
            ++Classes;
        }
        std::fclose(File);
        MTFE::Log::Info("dumped {} properties of {} classes ({} partial) -> {}", PropertyCount, Classes, Crashed, Path);
    }

    void DumpLive(std::uintptr_t Address, FILE* File)
    {
        char Prefix[160];
        if (!IsMtObject(Address))
        {
            std::fprintf(File, "%08X\t?\t#bad address\n", static_cast<unsigned>(Address));
            return;
        }
        auto* Object = reinterpret_cast<RE::MtObject*>(Address);
        const auto* Dti = Object->GetDTI();
        std::snprintf(Prefix, sizeof(Prefix), "%08X\t%s", static_cast<unsigned>(Address), Dti ? SafeName(Dti->GetName()) : "?");
        RE::MtPropertyList List;
        if (!List.Collect(Object))
        {
            std::fprintf(File, "%s\t#partial (createProperty crashed)\n", Prefix);
        }
        Write(File, Prefix, List, Address, Dti ? Dti->GetSize() : 0x10000);
    }

    void PollRequests()
    {
        static DWORD Last = 0;
        const auto Now = GetTickCount();
        if (Now - Last < 500)
        {
            return;
        }
        Last = Now;

        const auto Request = MTFE::GetMTFEPath() + "propdump.txt";
        FILE* In = _fsopen(Request.c_str(), "r", _SH_DENYNO);
        if (!In)
        {
            return;
        }
        std::vector<std::uintptr_t> Addresses;
        char Line[64];
        while (std::fgets(Line, sizeof(Line), In))
        {
            if (const auto Value = std::strtoul(Line, nullptr, 16))
            {
                Addresses.push_back(Value);
            }
        }
        std::fclose(In);
        DeleteFileA(Request.c_str());

        const auto Out = MTFE::GetMTFEPath() + "Logs\\properties_live.tsv";
        if (FILE* File = _fsopen(Out.c_str(), "a", _SH_DENYWR))
        {
            for (auto Address : Addresses)
            {
                DumpLive(Address, File);
            }
            std::fclose(File);
        }
        MTFE::Log::Info("live dump of {} object(s) -> {}", Addresses.size(), Out);
    }

    void OnMessage(MTFE::Message* Msg)
    {
        if (Msg->type == MTFE_MESSAGE_GAME_INIT)
        {
            MTFE::AddTask(DumpAll);
            MTFE::AddFrameCallback(PollRequests);
        }
    }
}

MTFE_PLUGIN_LOAD(const MTFE::LoadInterface* Interface)
{
    MTFE::Init(Interface);
    return MTFE::RegisterListener(OnMessage);
}
