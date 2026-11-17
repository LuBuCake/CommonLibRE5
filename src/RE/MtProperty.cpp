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

#include "RE/MT/MtProperty.h"

#include "RE/Offsets.h"

#include <Windows.h>

namespace RE
{
    namespace
    {
        int SafeCreateProperty(std::uintptr_t Function, MtObject* Object, MtPropertyList* List)
        {
            __try
            {
                REL::InvokeThiscall<void>(Function, Object, List);
                return 1;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return 0;
            }
        }
    }

    bool MtPropertyList::Collect(MtObject* Object)
    {
        const auto Function = reinterpret_cast<const std::uintptr_t*>(Object->GetVTable())[kCreateProperty];
        return SafeCreateProperty(Function, Object, this) != 0;
    }

    void MtPropertyList::Clear()
    {
        if (!last)
        {
            return;
        }
        auto* Lock = REL::Relocation<CRITICAL_SECTION*>(Offset::MtProperty::PoolLock).Get();
        auto** Head = REL::Relocation<MtProperty**>(Offset::MtProperty::FreeList).Get();
        EnterCriticalSection(Lock);
        for (MtProperty* Property = last; Property;)
        {
            MtProperty* Previous = Property->prev;
            Property->next = nullptr;
            Property->prev = *Head; // the pool's free list is linked through +0x24
            *Head = Property;
            Property = Previous;
        }
        LeaveCriticalSection(Lock);
        last = nullptr;
    }

    const MtProperty* MtPropertyList::First() const
    {
        const MtProperty* Property = last;
        while (Property && Property->prev)
        {
            Property = Property->prev;
        }
        return Property;
    }

    const MtProperty* MtPropertyList::Find(std::string_view Name) const
    {
        for (const MtProperty* Property = last; Property; Property = Property->prev)
        {
            if (Property->name && Name == Property->name)
            {
                return Property;
            }
        }
        return nullptr;
    }

    void* FindPropertyField(MtObject* Object, std::string_view Name)
    {
        MtPropertyList List;
        List.Collect(Object);
        const auto* Property = List.Find(Name);
        return Property && Property->IsField() ? reinterpret_cast<void*>(Property->get) : nullptr;
    }
}
