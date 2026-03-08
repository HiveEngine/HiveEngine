#pragma once

#include <comb/default_allocator.h>

#include <wax/containers/string.h>
#include <wax/containers/string_view.h>
#include <wax/containers/vector.h>

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
            STRING,
            BOOL,
            INT,
            FLOAT,
            STRING_ARRAY
        };

        Type m_type{Type::STRING};
        wax::String m_str{};
        int64_t m_intVal{0};
        double m_floatVal{0.0};
        bool m_boolVal{false};
        wax::Vector<wax::String> m_array{};

        // -- Factory methods --

        [[nodiscard]] static HiveValue MakeString(comb::DefaultAllocator& alloc, wax::StringView s) {
            HiveValue v{};
            v.m_type = Type::STRING;
            v.m_str = wax::String{alloc, s};
            return v;
        }

        [[nodiscard]] static HiveValue MakeBool(bool b) {
            HiveValue v{};
            v.m_type = Type::BOOL;
            v.m_boolVal = b;
            return v;
        }

        [[nodiscard]] static HiveValue MakeInt(int64_t i) {
            HiveValue v{};
            v.m_type = Type::INT;
            v.m_intVal = i;
            return v;
        }

        [[nodiscard]] static HiveValue MakeFloat(double f) {
            HiveValue v{};
            v.m_type = Type::FLOAT;
            v.m_floatVal = f;
            return v;
        }

        [[nodiscard]] static HiveValue MakeStringArray(comb::DefaultAllocator& alloc) {
            HiveValue v{};
            v.m_type = Type::STRING_ARRAY;
            v.m_array = wax::Vector<wax::String>{alloc};
            return v;
        }

        // -- Accessors --

        [[nodiscard]] wax::StringView AsString() const noexcept { return m_str.View(); }
        [[nodiscard]] bool AsBool() const noexcept { return m_boolVal; }
        [[nodiscard]] int64_t AsInt() const noexcept { return m_intVal; }
        [[nodiscard]] double AsFloat() const noexcept { return m_floatVal; }
        [[nodiscard]] const wax::Vector<wax::String>& AsStringArray() const noexcept { return m_array; }

        void PushString(comb::DefaultAllocator& alloc, wax::StringView s) { m_array.PushBack(wax::String{alloc, s}); }
    };
} // namespace nectar
