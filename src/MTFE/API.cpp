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

#include "MTFE/API.h"
#include "MTFE/Log.h"

#include <Windows.h>

#include <share.h>

#include <cstdio>
#include <mutex>

namespace MTFE
{
    namespace
    {
        const LoadInterface* Loader = nullptr;
        MTFEPluginHandle Handle = MTFE_INVALID_PLUGIN_HANDLE;
        const MTFEMessagingInterface* Messaging = nullptr;
        const MTFETaskInterface* Tasks = nullptr;

        std::mutex LogLock;
        FILE* LogFile = nullptr;

        void RunOnce(void* Context)
        {
            auto* Function = static_cast<std::function<void()>*>(Context);
            (*Function)();
            delete Function;
        }

        void RunEvery(void* Context)
        {
            (*static_cast<std::function<void()>*>(Context))();
        }
    }

    void Init(const LoadInterface* Interface)
    {
        Loader = Interface;
        Handle = Interface->GetPluginHandle();
        Messaging = static_cast<const MTFEMessagingInterface*>(Interface->QueryInterface(MTFE_INTERFACE_MESSAGING));
        Tasks = static_cast<const MTFETaskInterface*>(Interface->QueryInterface(MTFE_INTERFACE_TASK));

        std::lock_guard Lock(LogLock);
        if (!LogFile)
        {
            const auto Path = std::string(Interface->GetMTFEPath()) + "Logs\\" + MTFEPlugin_Version.name + ".log";
            LogFile = _fsopen(Path.c_str(), "w", _SH_DENYWR); // readable while the game runs
        }
    }

    const LoadInterface* GetLoadInterface()
    {
        return Loader;
    }
    MTFEPluginHandle GetPluginHandle()
    {
        return Handle;
    }
    std::string_view GetPluginName()
    {
        return MTFEPlugin_Version.name;
    }
    std::uint32_t GetRuntimeVersion()
    {
        return Loader ? Loader->runtimeVersion : 0;
    }
    std::string GetGamePath()
    {
        return Loader ? Loader->GetGamePath() : "";
    }
    std::string GetMTFEPath()
    {
        return Loader ? Loader->GetMTFEPath() : "";
    }
    const MTFEMessagingInterface* GetMessagingInterface()
    {
        return Messaging;
    }
    const MTFETaskInterface* GetTaskInterface()
    {
        return Tasks;
    }

    bool RegisterListener(MTFEMessageCallback Callback)
    {
        return RegisterListener("MTFE", Callback);
    }

    bool RegisterListener(const char* Sender, MTFEMessageCallback Callback)
    {
        return Messaging && Messaging->RegisterListener(Handle, Sender, Callback) != 0;
    }

    void AddTask(std::function<void()> Task)
    {
        if (Tasks)
        {
            Tasks->AddTask(RunOnce, new std::function<void()>(std::move(Task)));
        }
    }

    void AddFrameCallback(std::function<void()> Task)
    {
        if (Tasks)
        {
            Tasks->AddFrameCallback(RunEvery, new std::function<void()>(std::move(Task)));
        }
    }

    namespace Log
    {
        void Write(std::string_view Level, std::string_view Text)
        {
            SYSTEMTIME Time;
            GetLocalTime(&Time);
            const auto Line = std::format("[{:02}:{:02}:{:02}.{:03}] [{}] {}\n", Time.wHour, Time.wMinute, Time.wSecond,
                                          Time.wMilliseconds, Level, Text);
            std::lock_guard Lock(LogLock);
            if (LogFile)
            {
                std::fwrite(Line.data(), 1, Line.size(), LogFile);
                std::fflush(LogFile);
            }
            OutputDebugStringA(Line.c_str());
        }
    }
}
