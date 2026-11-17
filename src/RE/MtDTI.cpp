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

#include "RE/MT/MtDTI.h"

#include "RE/IDs_DTI.h"
#include "RE/Offsets.h"

namespace RE
{
    bool MtDTI::IsKindOf(const MtDTI* Other) const
    {
        for (const MtDTI* Dti = this; Dti; Dti = Dti->parent)
        {
            if (Dti == Other)
            {
                return true;
            }
        }
        return false;
    }

    const MtDTI* MtDTI::GetRoot()
    {
        return REL::Relocation<const MtDTI*>(Offset::MtDTI::Root).Get();
    }

    const MtDTI* MtDTI::Find(std::string_view ClassName)
    {
        const MtDTI* Found = nullptr;
        const auto Visit = [&](const MtDTI* Dti)
        {
            if (Dti->name && ClassName == Dti->name)
            {
                Found = Dti;
                return false;
            }
            return true;
        };
        ForEach(Visit);
        return Found;
    }
}
