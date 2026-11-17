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

#include "RE/Game/Damage.h"

namespace RE::Damage
{
    bool HookEnemyApplyDamage(void(__fastcall* Hook)(uCharacter*, void*, HitInfo*, std::int32_t), EnemyApplyDamage_t& Original)
    {
        const auto Call = EnemyApplyDamageCall.GetAddress();
        if (*reinterpret_cast<const std::uint8_t*>(Call) != 0xE8)
        {
            return false;
        }
        const auto Previous = REL::WriteCall5(Call, reinterpret_cast<std::uintptr_t>(Hook));
        Original = reinterpret_cast<EnemyApplyDamage_t>(Previous);
        return Previous != 0;
    }
}
