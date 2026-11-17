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

#include "RE/MT/MtObject.h"

#include <cstdint>
#include <string_view>

namespace RE
{
    // MT reflection: every class registers its fields in createProperty (vtable slot 3) as MtProperty entries
    // (name, type, field address or getter/setter). Verified on the Steam 2023 runtime:
    //   addProperty 0x422C00 (__thiscall on the list, copies a 0x28-byte entry, links it at list+4)
    //   entries come from a static pool: array 0x124C468, free list 0x12496A8 (linked through +0x24), lock 0x124C44C.
    //   The pool counter has no bound: entries must be given back (MtPropertyList does it in its destructor).
    enum class MtPropType : std::uint16_t
    {
        kClass = 1,
        kClassRef = 2,
        kBool = 3,
        kU8 = 4,
        kU16 = 5,
        kU32 = 6,
        kU64 = 7,
        kS8 = 8,
        kS16 = 9,
        kS32 = 10,
        kS64 = 11,
        kF32 = 12,
        kF64 = 13,
        kString = 14,
        kColor = 15,
        kPoint = 16,
        kSize = 17,
        kRect = 18,
        kMatrix = 19,
        kVector3 = 20,
        kVector4 = 21,
        kQuaternion = 22,
        kGroupBegin = 25, // editor label, no data
        kEnd = 31,
    };

    struct MtProperty
    {
        enum Attr : std::uint16_t
        {
            kGetSet = 0x80, // `get`/`set` are accessor functions, not a field address
        };

        bool IsField() const { return !(attr & kGetSet) && get != 0; }
        std::uintptr_t FieldOffset() const { return get - reinterpret_cast<std::uintptr_t>(owner); } // only if IsField()

        // members
        const char* name;    // 00
        MtPropType type;     // 04
        std::uint16_t attr;  // 06
        void* owner;         // 08 the object createProperty ran on
        std::uintptr_t get;  // 0C field address, or getter
        std::uint32_t count; // 10 array length / index
        std::uintptr_t set;  // 14 setter
        std::uint32_t unk18; // 18
        std::uint32_t unk1C; // 1C
        MtProperty* next;    // 20
        MtProperty* prev;    // 24
    };
    static_assert(sizeof(MtProperty) == 0x28);

    class MtPropertyList
    {
    public:
        static constexpr std::size_t kCreateProperty = 3;

        MtPropertyList() = default;
        MtPropertyList(const MtPropertyList&) = delete;
        MtPropertyList& operator=(const MtPropertyList&) = delete;
        ~MtPropertyList() { Clear(); }

        // Runs Object's createProperty on Object itself. false = it threw (entries registered before that are kept).
        bool Collect(MtObject* Object);
        void Clear(); // gives every entry back to the game's pool

        const MtProperty* First() const;
        const MtProperty* Last() const { return last; }
        const MtProperty* Find(std::string_view Name) const;

        template <class F>
        void ForEach(F&& Visit) const
        {
            for (const MtProperty* Property = First(); Property; Property = Property->next)
            {
                Visit(*Property);
            }
        }

        // members (the game's layout: addProperty only touches +4)
        std::uint32_t unk0 = 0;
        MtProperty* last = nullptr;
    };

    // Address of a field registered as a property (nullptr if missing or accessor-only). Builds and frees a list.
    void* FindPropertyField(MtObject* Object, std::string_view Name);

    template <class T>
    T* GetPropertyField(MtObject* Object, std::string_view Name)
    {
        return static_cast<T*>(FindPropertyField(Object, Name));
    }
}
