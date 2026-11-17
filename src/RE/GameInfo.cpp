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

#include "RE/Game/GameInfo.h"

namespace RE
{
    const CostumeInfo* FindCostume(CharacterID Character, std::int32_t Costume)
    {
        for (const auto& Info : kCostumes)
        {
            if (Info.Character == Character && Info.Costume == Costume)
            {
                return &Info;
            }
        }
        return nullptr;
    }

    const CostumeInfo* FindCostumeByClass(std::string_view PlayerClass)
    {
        for (const auto& Info : kCostumes)
        {
            if (PlayerClass == Info.PlayerClass)
            {
                return &Info;
            }
        }
        return nullptr;
    }
}
