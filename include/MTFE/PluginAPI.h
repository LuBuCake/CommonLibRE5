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

// MTFE plugin ABI. Plain C, x86 (__cdecl exports), shared by the loader (dinput8.dll) and CommonLibRE5.
// Only append to these structs; never reorder or remove fields.

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define MTFE_MAKE_VERSION(major, minor, patch) ((((uint32_t)(major)) << 24) | (((uint32_t)(minor)) << 16) | ((uint32_t)(patch) << 4))
#define MTFE_VERSION_MAJOR(v) (((v) >> 24) & 0xFF)
#define MTFE_VERSION_MINOR(v) (((v) >> 16) & 0xFF)
#define MTFE_VERSION_PATCH(v) (((v) >> 4) & 0xFFF)

#define MTFE_VERSION MTFE_MAKE_VERSION(1, 0, 0)

// Game runtimes are identified by the PE TimeDateStamp of re5dx9.exe.
#define MTFE_RUNTIME_RE5_STEAM_2023 0x63E9CE00u // Steam, Gold Edition, Enigma-protected update (build Jan 23 2023)

    typedef uint32_t MTFEPluginHandle;
#define MTFE_INVALID_PLUGIN_HANDLE 0xFFFFFFFFu

// Exported by every plugin as data: `extern "C" __declspec(dllexport) MTFEPluginVersionData MTFEPlugin_Version`.
#define MTFE_PLUGIN_VERSION_DATA_VERSION 1u

    enum MTFEAddressIndependence
    {
        MTFE_ADDRESS_INDEPENDENT_SIGNATURES = 1u << 0, // only pattern scans, works on any runtime
    };

    typedef struct MTFEPluginVersionData
    {
        uint32_t dataVersion;   // MTFE_PLUGIN_VERSION_DATA_VERSION
        uint32_t pluginVersion; // MTFE_MAKE_VERSION
        char name[64];
        char author[64];
        uint32_t addressIndependence;    // MTFEAddressIndependence
        uint32_t compatibleVersions[16]; // runtimes (MTFE_RUNTIME_*), 0-terminated; ignored when address independent
        uint32_t mtfeVersionRequired;    // minimum MTFE_VERSION
        uint32_t reserved[16];
    } MTFEPluginVersionData;

    enum MTFEInterfaceID
    {
        MTFE_INTERFACE_INVALID = 0,
        MTFE_INTERFACE_MESSAGING = 1,
        MTFE_INTERFACE_TASK = 2,
        MTFE_INTERFACE_MENU = 3,
    };

    typedef struct MTFEPluginInfo
    {
        uint32_t infoVersion;
        const char* name;
        uint32_t version;
    } MTFEPluginInfo;

    // Passed to MTFEPlugin_Load.
    typedef struct MTFELoadInterface
    {
        uint32_t mtfeVersion;
        uint32_t runtimeVersion; // MTFE_RUNTIME_* of the running game
        uint32_t isEditor;       // always 0
        void* (*QueryInterface)(uint32_t id);
        MTFEPluginHandle (*GetPluginHandle)(void);
        const MTFEPluginInfo* (*GetPluginInfo)(const char* name);
        const char* (*GetGamePath)(void); // game folder, with trailing backslash
        const char* (*GetMTFEPath)(void); // <game>\MTFE\, with trailing backslash
    } MTFELoadInterface;

    // Messages sent by the sender "MTFE".
    enum MTFEMessageType
    {
        MTFE_MESSAGE_POST_LOAD = 0,      // every plugin got MTFEPlugin_Load (at the game OEP, before the CRT starts)
        MTFE_MESSAGE_POST_POST_LOAD = 1, // after POST_LOAD was handled by everyone
        MTFE_MESSAGE_PRE_WINMAIN = 2,    // CRT and static initializers done: the MtDTI tree exists, no game systems yet
        MTFE_MESSAGE_GAME_INIT = 3,      // first frame of the main loop: sMain and the systems exist, window/device created
    };

    typedef struct MTFEMessage
    {
        const char* sender;
        uint32_t type;
        uint32_t dataLen;
        void* data;
    } MTFEMessage;

    typedef void (*MTFEMessageCallback)(MTFEMessage* msg);

#define MTFE_MESSAGING_INTERFACE_VERSION 1u

    typedef struct MTFEMessagingInterface
    {
        uint32_t interfaceVersion;
        // sender = NULL listens to every sender; "MTFE" = loader messages; otherwise a plugin name.
        uint32_t (*RegisterListener)(MTFEPluginHandle listener, const char* sender, MTFEMessageCallback handler);
        // receiver = NULL broadcasts to every listener of this plugin.
        uint32_t (*Dispatch)(MTFEPluginHandle sender, uint32_t type, void* data, uint32_t dataLen, const char* receiver);
    } MTFEMessagingInterface;

    typedef void (*MTFETaskFunction)(void* context);

#define MTFE_TASK_INTERFACE_VERSION 1u

    // Tasks run on the game's main thread, at the start of sMain's per-frame update.
    typedef struct MTFETaskInterface
    {
        uint32_t interfaceVersion;
        void (*AddTask)(MTFETaskFunction fn, void* context);          // runs once, next frame (thread safe)
        void (*AddFrameCallback)(MTFETaskFunction fn, void* context); // runs every frame from now on (thread safe)
    } MTFETaskInterface;

    // Mod settings in the game's own Options menu (Options > MODS > <plugin page>). Each plugin gets one page; every
    // control with a key is saved as an integer in <game>\MTFE\Config\<plugin name>.ini, section [Settings], and loaded
    // when the control is added (never in the game save). Texts are ASCII 0x20..0x7E (the menu font has no accents).
    typedef uint32_t MTFEMenuControl;
#define MTFE_INVALID_MENU_CONTROL 0u

    enum MTFEMenuControlType
    {
        MTFE_MENU_HEADER = 0, // a text row, not selectable (label only)
        MTFE_MENU_TOGGLE = 1, // value 0 / 1, shown as OFF / ON
        MTFE_MENU_LIST = 2,   // value = index into items[]
        MTFE_MENU_NUMBER = 3, // value in [minValue, maxValue] by step, shown as value / 10^decimals
        MTFE_MENU_HOTKEY = 4, // value = game key code (0 = none; keyboard 1..0x61, mouse 0x62..0x66), set by pressing a key
        MTFE_MENU_BUTTON = 5, // no value: onChange(control, 0, context) when the player confirms it
    };

    // Main thread when the player changes a value; the caller's thread for SetValue.
    typedef void (*MTFEMenuCallback)(MTFEMenuControl control, int32_t value, void* context);

    typedef struct MTFEMenuControlDesc
    {
        uint32_t size;     // sizeof(MTFEMenuControlDesc): fields are only appended
        uint32_t type;     // MTFEMenuControlType
        const char* key;   // INI key; NULL = not saved (HEADER and BUTTON never are)
        const char* label; // row text
        const char* help;  // text at the bottom while the row is selected (may be NULL)
        int32_t defaultValue;
        int32_t minValue;          // NUMBER
        int32_t maxValue;          // NUMBER
        int32_t step;              // NUMBER (0 = 1)
        uint32_t decimals;         // NUMBER: 1 shows 15 as 1.5
        const char* const* items;  // LIST
        uint32_t itemCount;        // LIST
        MTFEMenuCallback onChange; // may be NULL
        void* context;
    } MTFEMenuControlDesc;

#define MTFE_MENU_INTERFACE_VERSION 1u

    typedef struct MTFEMenuInterface
    {
        uint32_t interfaceVersion;
        // Title of the plugin's page and of its row in the MODS list (default: the plugin name).
        void (*SetPageTitle)(MTFEPluginHandle plugin, const char* title);
        // Appends a row to the plugin's page; strings are copied. Returns MTFE_INVALID_MENU_CONTROL on a bad desc.
        MTFEMenuControl (*AddControl)(MTFEPluginHandle plugin, const MTFEMenuControlDesc* desc);
        int32_t (*GetValue)(MTFEMenuControl control);
        // Clamps, saves and calls onChange when the value changes. Thread safe.
        void (*SetValue)(MTFEMenuControl control, int32_t value);
    } MTFEMenuInterface;

    // Plugin exports:
    //   MTFEPluginVersionData MTFEPlugin_Version;                 (data, required)
    //   bool MTFEPlugin_Load(const MTFELoadInterface* mtfe);     (required; false = unload)
    typedef uint8_t (*MTFEPlugin_LoadFunc)(const MTFELoadInterface* mtfe);

#ifdef __cplusplus
}
#endif
