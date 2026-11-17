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

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string_view>
#include <type_traits>

namespace REL
{
    // The running re5dx9.exe.
    class Module
    {
    public:
        static const Module& Get();

        std::uintptr_t GetBase() const { return _base; }
        std::uint32_t GetRuntime() const { return _timestamp; } // MTFE_RUNTIME_* (PE TimeDateStamp)
        std::uintptr_t GetTextBegin() const { return _textBegin; }
        std::uintptr_t GetTextEnd() const { return _textEnd; }
        bool IsSupported() const; // the runtime the generated IDs were made for

    private:
        Module();

        std::uintptr_t _base = 0;
        std::uint32_t _timestamp = 0;
        std::uintptr_t _textBegin = 0;
        std::uintptr_t _textEnd = 0;
    };

    // RVA in re5dx9.exe (Ghidra address - 0x400000).
    class Offset
    {
    public:
        constexpr Offset() = default;
        constexpr explicit Offset(std::uintptr_t Rva) : _rva(Rva) {}

        constexpr std::uintptr_t GetOffset() const { return _rva; }
        std::uintptr_t GetAddress() const { return Module::Get().GetBase() + _rva; }

    private:
        std::uintptr_t _rva = 0;
    };

    void SafeWrite(std::uintptr_t Address, const void* Data, std::size_t Size);

    template <class T>
    void SafeWrite(std::uintptr_t Address, const T& Value)
        requires std::is_trivially_copyable_v<T>
    {
        SafeWrite(Address, &Value, sizeof(T));
    }

    void SafeFill(std::uintptr_t Address, std::uint8_t Value, std::size_t Count);

    // Rewrite "call rel32" (E8) / "jmp rel32" (E9) at `Address` to go to `Target`.
    // Returns the previous destination: call it from the hook to keep other mods' hooks in the chain.
    std::uintptr_t WriteCall5(std::uintptr_t Address, std::uintptr_t Target);
    std::uintptr_t WriteBranch5(std::uintptr_t Address, std::uintptr_t Target);

    template <class T>
    class Relocation
    {
    public:
        using ValueType = std::conditional_t<std::is_member_pointer_v<T> || std::is_function_v<std::remove_pointer_t<T>>, std::decay_t<T>, T>;

        constexpr Relocation() = default;
        explicit Relocation(std::uintptr_t Address) : _address(Address) {}
        explicit Relocation(Offset Rva) : _address(Rva.GetAddress()) {}
        Relocation(Offset Rva, std::ptrdiff_t Delta) : _address(Rva.GetAddress() + Delta) {}

        std::uintptr_t GetAddress() const { return _address; }
        std::uintptr_t GetOffset() const { return _address - Module::Get().GetBase(); }

        ValueType Get() const
            requires std::is_pointer_v<ValueType>
        {
            return reinterpret_cast<ValueType>(_address);
        }

        template <class U = ValueType>
        std::add_lvalue_reference_t<std::remove_pointer_t<U>> operator*() const
            requires std::is_pointer_v<U> && !std::is_function_v<std::remove_pointer_t<U>>
        {
            return *Get();
        }

        template <class U = ValueType>
        U operator->() const
            requires std::is_pointer_v<U> && !std::is_function_v<std::remove_pointer_t<U>>
        {
            return Get();
        }

        template <class... Args>
        decltype(auto) operator()(Args&&... Arguments) const
            requires std::is_invocable_v<ValueType, Args...>
        {
            return Get()(std::forward<Args>(Arguments)...);
        }

        // this = a vtable; replaces slot `Index`, returns the previous entry (chain-preserving).
        std::uintptr_t WriteVFunc(std::size_t Index, std::uintptr_t Target) const
        {
            const auto Slot = _address + Index * sizeof(std::uintptr_t);
            const auto Previous = *reinterpret_cast<std::uintptr_t*>(Slot);
            SafeWrite(Slot, Target);
            return Previous;
        }

        template <class F>
        std::uintptr_t WriteVFunc(std::size_t Index, F Target) const
        {
            return WriteVFunc(Index, reinterpret_cast<std::uintptr_t>(Target));
        }

        std::uintptr_t WriteCall5(std::uintptr_t Target) const { return REL::WriteCall5(_address, Target); }
        std::uintptr_t WriteBranch5(std::uintptr_t Target) const { return REL::WriteBranch5(_address, Target); }

    private:
        std::uintptr_t _address = 0;
    };

    // IDA-style byte pattern ("E8 ?? ?? ?? ?? 84 C0") searched in the game's .text.
    std::optional<std::uintptr_t> FindPattern(std::string_view Pattern);
    std::optional<std::uintptr_t> FindPattern(std::string_view Pattern, std::uintptr_t Begin, std::uintptr_t End);

    // Destination of a "call/jmp rel32" at `Address`.
    std::uintptr_t FollowRel32(std::uintptr_t Address);

    template <class R, class... Args>
    R InvokeThiscall(std::uintptr_t Function, void* Self, Args... Arguments)
    {
        using F = R(__thiscall*)(void*, Args...);
        return reinterpret_cast<F>(Function)(Self, Arguments...);
    }

    // Call slot `Index` of `Self`'s vtable as __thiscall.
    template <class R, class... Args>
    R InvokeVFunc(const void* Self, std::size_t Index, Args... Arguments)
    {
        const auto VTable = *reinterpret_cast<const std::uintptr_t* const*>(Self);
        return InvokeThiscall<R>(VTable[Index], const_cast<void*>(Self), Arguments...);
    }
}
