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

#include "RE/IDs_VFuncs.h"
#include "RE/MT/MtDTI.h"
#include "RE/MT/MtField.h"

#include <utility>

namespace RE
{
    // Base of every MT Framework class with a DTI. The vtable is reached by index (REL::InvokeVFunc with the slots of
    // RE::VFUNC, named in data/VFuncs.tsv) instead of C++ virtuals, so an unknown slot can never be called by mistake.
    class MtObject
    {
    public:
        static constexpr std::size_t kGetDTI = VFUNC::MtObject::getDTI; // "mov eax, &DTI; ret" in every vtable of the tree

        const MtDTI* GetDTI() const { return REL::InvokeVFunc<const MtDTI*>(this, kGetDTI); }

        bool IsKindOf(const MtDTI* Dti) const
        {
            const auto* Mine = GetDTI();
            return Mine && Mine->IsKindOf(Dti);
        }

        template <class T>
        T* As()
        {
            return IsKindOf(T::DTI()) ? static_cast<T*>(this) : nullptr;
        }

        std::uintptr_t GetVTable() const { return *reinterpret_cast<const std::uintptr_t*>(this); }

        // Fields and properties by generated id (RE/IDs_Fields.h): Player->Get(RE::FIELD::uCharacter::Hp) = 1000.
        template <class Id, class... Args>
        decltype(auto) Get(Id Which, Args... Arguments)
        {
            return RE::Get(this, Which, Arguments...);
        }

        template <class Id, class... Args>
        decltype(auto) Get(Id Which, Args... Arguments) const
        {
            return RE::Get(this, Which, Arguments...);
        }

        template <class Id, class... Args>
        bool Set(Id Which, Args&&... Arguments)
        {
            return RE::Set(this, Which, std::forward<Args>(Arguments)...);
        }

        // members
        void* vtbl; // 00
    };
    static_assert(sizeof(MtObject) == 0x4);
}
