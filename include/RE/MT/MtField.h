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

// Typed access to the fields and properties of game objects (ids generated in RE/IDs_Fields.h):
//   auto& Pos = RE::Get(Player, RE::FIELD::uPlayer::mPos);         // MtVector3& (a field: a reference into the object)
//   float Rate = RE::Get(Player, RE::PROP::uPlayer::LightMapBlendRate); // a property: calls the game's getter
//   RE::Set(Player, RE::PROP::uPlayer::LightMapBlendRate, 0.5f);       // ... and its setter
// MtObject has the same as members (Player->Get(...)). Accessor conventions, checked in the disassembly and against
// the `ret N` of every getter/setter (tools/gen_fields.py drops the ones that do not match):
//   value:   T get()              void set(T)              (__thiscall)
//   pointer: void get(T* out)     void set(const T* in)    (vectors, matrices, colors, rects)
//   arrays:  the same with a trailing std::uint32_t index (get(index) / set(value, index), get(out, index)...)

#include "REL/Relocation.h"
#include "RE/MT/MtMath.h"

#include <cstdint>
#include <type_traits>

namespace RE
{
    class MtObject;

    // Field at a fixed offset of an object, for types only partially mapped.
    template <class T>
    T& Field(void* Object, std::uintptr_t ByteOffset)
    {
        return *reinterpret_cast<T*>(reinterpret_cast<std::uintptr_t>(Object) + ByteOffset);
    }

    template <class T>
    const T& Field(const void* Object, std::uintptr_t ByteOffset)
    {
        return *reinterpret_cast<const T*>(reinterpret_cast<std::uintptr_t>(Object) + ByteOffset);
    }

    // A field: its offset in the object. T = void when the type is not known (RE::Get gives the address).
    template <class T>
    struct FieldID
    {
        std::uint32_t Offset;
    };

    enum class PropKind
    {
        kValue,
        kPointer,
        kValueArray,
        kPointerArray,
    };

    // A property read and written through the game's accessor functions. Setter = 0: read-only. The accessors run on
    // the object that registered the property (MtProperty::owner): the object itself, or a sub-object at ThisOffset
    // (uModel::RenderInfo's properties belong to a part of the model, not to the unit).
    template <class T, PropKind Kind>
    struct PropID
    {
        REL::Offset Getter;
        REL::Offset Setter;
        std::uint32_t ThisOffset = 0;

        constexpr bool CanSet() const { return Setter.GetOffset() != 0; }
        const void* This(const void* Object) const { return reinterpret_cast<const std::uint8_t*>(Object) + ThisOffset; }
        void* This(void* Object) const { return reinterpret_cast<std::uint8_t*>(Object) + ThisOffset; }
    };

    template <class T>
    T& Get(void* Object, FieldID<T> Id)
        requires(!std::is_void_v<T>)
    {
        return Field<T>(Object, Id.Offset);
    }

    template <class T>
    const T& Get(const void* Object, FieldID<T> Id)
        requires(!std::is_void_v<T>)
    {
        return Field<T>(Object, Id.Offset);
    }

    inline void* Get(void* Object, FieldID<void> Id)
    {
        return reinterpret_cast<void*>(reinterpret_cast<std::uintptr_t>(Object) + Id.Offset);
    }

    template <class T>
    T Get(const void* Object, PropID<T, PropKind::kValue> Id)
    {
        using Func = T(__thiscall*)(const void*);
        return reinterpret_cast<Func>(Id.Getter.GetAddress())(Id.This(Object));
    }

    template <class T>
    T Get(const void* Object, PropID<T, PropKind::kPointer> Id)
    {
        using Func = void(__thiscall*)(const void*, T*);
        T Value{};
        reinterpret_cast<Func>(Id.Getter.GetAddress())(Id.This(Object), &Value);
        return Value;
    }

    template <class T>
    T Get(const void* Object, PropID<T, PropKind::kValueArray> Id, std::uint32_t Index)
    {
        using Func = T(__thiscall*)(const void*, std::uint32_t);
        return reinterpret_cast<Func>(Id.Getter.GetAddress())(Id.This(Object), Index);
    }

    template <class T>
    T Get(const void* Object, PropID<T, PropKind::kPointerArray> Id, std::uint32_t Index)
    {
        using Func = void(__thiscall*)(const void*, T*, std::uint32_t);
        T Value{};
        reinterpret_cast<Func>(Id.Getter.GetAddress())(Id.This(Object), &Value, Index);
        return Value;
    }

    // Returns false for a read-only property.
    template <class T>
    bool Set(void* Object, PropID<T, PropKind::kValue> Id, std::type_identity_t<T> Value)
    {
        using Func = void(__thiscall*)(void*, T);
        return Id.CanSet() && (reinterpret_cast<Func>(Id.Setter.GetAddress())(Id.This(Object), Value), true);
    }

    template <class T>
    bool Set(void* Object, PropID<T, PropKind::kPointer> Id, const std::type_identity_t<T>& Value)
    {
        using Func = void(__thiscall*)(void*, const T*);
        return Id.CanSet() && (reinterpret_cast<Func>(Id.Setter.GetAddress())(Id.This(Object), &Value), true);
    }

    template <class T>
    bool Set(void* Object, PropID<T, PropKind::kValueArray> Id, std::type_identity_t<T> Value, std::uint32_t Index)
    {
        using Func = void(__thiscall*)(void*, T, std::uint32_t);
        return Id.CanSet() && (reinterpret_cast<Func>(Id.Setter.GetAddress())(Id.This(Object), Value, Index), true);
    }

    template <class T>
    bool Set(void* Object, PropID<T, PropKind::kPointerArray> Id, const std::type_identity_t<T>& Value, std::uint32_t Index)
    {
        using Func = void(__thiscall*)(void*, const T*, std::uint32_t);
        return Id.CanSet() && (reinterpret_cast<Func>(Id.Setter.GetAddress())(Id.This(Object), &Value, Index), true);
    }
}
