#pragma once

#include <wax/containers/string.h>
#include <wax/containers/string_view.h>
#include <wax/containers/vector.h>
#include <comb/default_allocator.h>
#include <cstdint>

namespace nectar
{
    /// Tagged value for a .hive file entry.
    /// Simple struct — all fields present, only the one matching `type` is meaningful.
    /// .hive files have ~20-30 entries max, so memory waste is negligible.
    struct HiveValue
    {
        enum class Type : uint8_t
        {
            String,
            Bool,
            Int,
            Float,
            StringArray
        };

        Type type{Type::String};
        wax::String<> str{};
        int64_t int_val{0};
        double float_val{0.0};
        bool bool_val{false};
        wax::Vector<wax::String<>> array{};

        // -- Factory methods --

        [[nodiscard]] static HiveValue MakeString(comb::DefaultAllocator& alloc, wax::StringView s)
        {
            HiveValue v{};
            v.type = Type::String;
            v.str = wax::String<>{alloc, s};
            return v;
        }

        [[nodiscard]] static HiveValue MakeBool(bool b)
        {
            HiveValue v{};
            v.type = Type::Bool;
            v.bool_val = b;
            return v;
        }

        [[nodiscard]] static HiveValue MakeInt(int64_t i)
        {
            HiveValue v{};
            v.type = Type::Int;
            v.int_val = i;
            return v;
        }

        [[nodiscard]] static HiveValue MakeFloat(double f)
        {
            HiveValue v{};
            v.type = Type::Float;
            v.float_val = f;
            return v;
        }

        [[nodiscard]] static HiveValue MakeStringArray(comb::DefaultAllocator& alloc)
        {
            HiveValue v{};
            v.type = Type::StringArray;
            v.array = wax::Vector<wax::String<>>{alloc};
            return v;
        }

        // -- Accessors --

        [[nodiscard]] wax::StringView AsString() const noexcept { return str.View(); }
        [[nodiscard]] bool AsBool() const noexcept { return bool_val; }
        [[nodiscard]] int64_t AsInt() const noexcept { return int_val; }
        [[nodiscard]] double AsFloat() const noexcept { return float_val; }
        [[nodiscard]] const wax::Vector<wax::String<>>& AsStringArray() const noexcept { return array; }

        void PushString(comb::DefaultAllocator& alloc, wax::StringView s)
        {
            array.PushBack(wax::String<>{alloc, s});
        }
    };
}
