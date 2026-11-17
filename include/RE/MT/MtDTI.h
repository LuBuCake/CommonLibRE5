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

#include "REL/Relocation.h"

#include <cstdint>
#include <string_view>
#include <vector>

namespace RE
{
    class MtObject;

    // MT Framework run-time type info: one static instance per class, linked into a tree by static initializers.
    // Layout verified on the Steam 2023 runtime (see _Docs/Engenharia Reversa (Ghidra).md).
    class MtDTI
    {
    public:
        // vfunc 1 of the DTI's own vtable: allocates and constructs a new object of the class (used by the app init
        // for sMain).
        MtObject* NewInstance() const { return REL::InvokeVFunc<MtObject*>(this, 1); }

        const char* GetName() const { return name; }
        std::uint32_t GetSize() const { return sizeFlags & 0x7FFFFF; }
        std::uint32_t GetFlags() const { return sizeFlags >> 23; }
        std::uint32_t GetID() const { return id; }
        const MtDTI* GetParent() const { return parent; }
        const MtDTI* GetFirstChild() const { return child; }
        const MtDTI* GetNextSibling() const { return next; }

        bool IsKindOf(const MtDTI* Other) const;

        static const MtDTI* GetRoot();
        static const MtDTI* Find(std::string_view ClassName); // walks the whole tree; cache the result

        // Visits every class under (and including) `From`; stop by returning false.
        template <class F>
        static void ForEach(F&& Visit, const MtDTI* From = nullptr);

        // members
        void* vtbl;              // 00
        const char* name;        // 04
        const MtDTI* next;       // 08
        const MtDTI* child;      // 0C
        const MtDTI* parent;     // 10
        const MtDTI* link;       // 14
        std::uint32_t unk18;     // 18
        std::uint32_t sizeFlags; // 1C
        std::uint32_t id;        // 20 CRC of the name
    };
    static_assert(sizeof(MtDTI) == 0x24);

    template <class F>
    void MtDTI::ForEach(F&& Visit, const MtDTI* From)
    {
        std::vector<const MtDTI*> Stack{ From ? From : GetRoot() };
        while (!Stack.empty())
        {
            const MtDTI* Dti = Stack.back();
            Stack.pop_back();
            if (!Visit(Dti))
            {
                return;
            }
            for (const MtDTI* Child = Dti->child; Child; Child = Child->next)
            {
                Stack.push_back(Child);
            }
        }
    }
}
