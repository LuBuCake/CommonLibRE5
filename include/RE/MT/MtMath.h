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

// Value types of the MT properties (MtPropType 15..22). Vector3/Vector4/Quaternion: [code] getters copy x, y, z (w) as
// floats; 16-byte aligned like the fields they come from. The others are known by size only (members not checked).

#include <cstdint>

namespace RE
{
    struct alignas(16) MtVector3
    {
        float x, y, z, w;
    };
    static_assert(sizeof(MtVector3) == 0x10);

    struct alignas(16) MtVector4
    {
        float x, y, z, w;
    };
    static_assert(sizeof(MtVector4) == 0x10);

    struct alignas(16) MtQuaternion
    {
        float x, y, z, w;
    };
    static_assert(sizeof(MtQuaternion) == 0x10);

    struct alignas(16) MtMatrix
    {
        float m[4][4];
    };
    static_assert(sizeof(MtMatrix) == 0x40);

    struct MtColor
    {
        std::uint32_t value; // 4 bytes; channel order not checked
    };
    static_assert(sizeof(MtColor) == 0x4);

    struct MtPoint
    {
        std::int32_t x, y;
    };
    static_assert(sizeof(MtPoint) == 0x8);

    struct MtSize
    {
        std::int32_t w, h;
    };
    static_assert(sizeof(MtSize) == 0x8);

    struct MtRect
    {
        std::int32_t l, t, r, b;
    };
    static_assert(sizeof(MtRect) == 0x10);
}
