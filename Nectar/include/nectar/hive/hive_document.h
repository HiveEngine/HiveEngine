#pragma once

#include <nectar/hive/hive_value.h>
#include <wax/containers/hash_map.h>
#include <wax/containers/string.h>
#include <wax/containers/string_view.h>
#include <comb/default_allocator.h>

namespace nectar
{
    /// Parsed representation of a .hive file.
    /// Sections are stored flat with their full dotted name (e.g. "import.platform.mobile").
    /// Each section maps string keys to HiveValue entries.
    class HiveDocument
    {
    public:
        using SectionMap = wax::HashMap<wax::String<>, HiveValue>;

        explicit HiveDocument(comb::DefaultAllocator& alloc)
            : alloc_{&alloc}
            , sections_{alloc, 16}
        {}

        // -- Section management --

        [[nodiscard]] bool HasSection(wax::StringView name) const
        {
            wax::String<> key{*alloc_, name};
            return sections_.Contains(key);
        }

        void AddSection(wax::StringView name)
        {
            wax::String<> key{*alloc_, name};
            if (!sections_.Contains(key))
            {
                sections_.Insert(static_cast<wax::String<>&&>(key), SectionMap{*alloc_, 8});
            }
        }

        // -- Value access --

        void SetValue(wax::StringView section, wax::StringView key, HiveValue value)
        {
            wax::String<> sec_key{*alloc_, section};
            auto* sec = sections_.Find(sec_key);
            if (!sec)
            {
                sections_.Insert(wax::String<>{*alloc_, section}, SectionMap{*alloc_, 8});
                sec = sections_.Find(sec_key);
            }
            wax::String<> val_key{*alloc_, key};
            auto* existing = sec->Find(val_key);
            if (existing)
            {
                *existing = static_cast<HiveValue&&>(value);
            }
            else
            {
                sec->Insert(static_cast<wax::String<>&&>(val_key), static_cast<HiveValue&&>(value));
            }
        }

        [[nodiscard]] const HiveValue* GetValue(wax::StringView section, wax::StringView key) const
        {
            wax::String<> sec_key{*alloc_, section};
            auto* sec = sections_.Find(sec_key);
            if (!sec) return nullptr;
            wax::String<> val_key{*alloc_, key};
            return sec->Find(val_key);
        }

        [[nodiscard]] HiveValue* GetValue(wax::StringView section, wax::StringView key)
        {
            wax::String<> sec_key{*alloc_, section};
            auto* sec = sections_.Find(sec_key);
            if (!sec) return nullptr;
            wax::String<> val_key{*alloc_, key};
            return sec->Find(val_key);
        }

        // -- Convenience getters with defaults --

        [[nodiscard]] wax::StringView GetString(wax::StringView section, wax::StringView key,
                                                 wax::StringView fallback = {}) const
        {
            auto* v = GetValue(section, key);
            if (!v || v->type != HiveValue::Type::String) return fallback;
            return v->AsString();
        }

        [[nodiscard]] bool GetBool(wax::StringView section, wax::StringView key,
                                    bool fallback = false) const
        {
            auto* v = GetValue(section, key);
            if (!v || v->type != HiveValue::Type::Bool) return fallback;
            return v->AsBool();
        }

        [[nodiscard]] int64_t GetInt(wax::StringView section, wax::StringView key,
                                      int64_t fallback = 0) const
        {
            auto* v = GetValue(section, key);
            if (!v || v->type != HiveValue::Type::Int) return fallback;
            return v->AsInt();
        }

        [[nodiscard]] double GetFloat(wax::StringView section, wax::StringView key,
                                       double fallback = 0.0) const
        {
            auto* v = GetValue(section, key);
            if (!v || v->type != HiveValue::Type::Float) return fallback;
            return v->AsFloat();
        }

        // -- Iteration --

        using DocumentMap = wax::HashMap<wax::String<>, SectionMap>;

        [[nodiscard]] const DocumentMap& Sections() const noexcept { return sections_; }
        [[nodiscard]] DocumentMap& Sections() noexcept { return sections_; }

        [[nodiscard]] comb::DefaultAllocator& GetAllocator() const noexcept { return *alloc_; }

    private:
        comb::DefaultAllocator* alloc_;
        DocumentMap sections_;
    };
}
