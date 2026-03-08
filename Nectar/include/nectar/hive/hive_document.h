#pragma once

#include <comb/default_allocator.h>

#include <wax/containers/hash_map.h>
#include <wax/containers/string.h>
#include <wax/containers/string_view.h>

#include <nectar/hive/hive_value.h>

namespace nectar
{
    /// Parsed representation of a .hive file.
    /// Sections are stored flat with their full dotted name (e.g. "import.platform.mobile").
    /// Each section maps string keys to HiveValue entries.
    class HiveDocument
    {
    public:
        using SectionMap = wax::HashMap<wax::String, HiveValue>;

        explicit HiveDocument(comb::DefaultAllocator& alloc)
            : m_alloc{&alloc}
            , m_sections{alloc, 16} {}

        // -- Section management --

        [[nodiscard]] bool HasSection(wax::StringView name) const {
            wax::String key{*m_alloc, name};
            return m_sections.Contains(key);
        }

        void AddSection(wax::StringView name) {
            wax::String key{*m_alloc, name};
            if (!m_sections.Contains(key))
            {
                m_sections.Insert(static_cast<wax::String&&>(key), SectionMap{*m_alloc, 8});
            }
        }

        // -- Value access --

        void SetValue(wax::StringView section, wax::StringView key, HiveValue value) {
            wax::String secKey{*m_alloc, section};
            auto* sec = m_sections.Find(secKey);
            if (!sec)
            {
                m_sections.Insert(wax::String{*m_alloc, section}, SectionMap{*m_alloc, 8});
                sec = m_sections.Find(secKey);
            }
            wax::String valKey{*m_alloc, key};
            auto* existing = sec->Find(valKey);
            if (existing)
            {
                *existing = static_cast<HiveValue&&>(value);
            }
            else
            {
                sec->Insert(static_cast<wax::String&&>(valKey), static_cast<HiveValue&&>(value));
            }
        }

        [[nodiscard]] const HiveValue* GetValue(wax::StringView section, wax::StringView key) const {
            wax::String secKey{*m_alloc, section};
            auto* sec = m_sections.Find(secKey);
            if (!sec)
                return nullptr;
            wax::String valKey{*m_alloc, key};
            return sec->Find(valKey);
        }

        [[nodiscard]] HiveValue* GetValue(wax::StringView section, wax::StringView key) {
            wax::String secKey{*m_alloc, section};
            auto* sec = m_sections.Find(secKey);
            if (!sec)
                return nullptr;
            wax::String valKey{*m_alloc, key};
            return sec->Find(valKey);
        }

        // -- Convenience getters with defaults --

        [[nodiscard]] wax::StringView GetString(wax::StringView section, wax::StringView key,
                                                wax::StringView fallback = {}) const {
            auto* v = GetValue(section, key);
            if (!v || v->m_type != HiveValue::Type::STRING)
                return fallback;
            return v->AsString();
        }

        [[nodiscard]] bool GetBool(wax::StringView section, wax::StringView key, bool fallback = false) const {
            auto* v = GetValue(section, key);
            if (!v || v->m_type != HiveValue::Type::BOOL)
                return fallback;
            return v->AsBool();
        }

        [[nodiscard]] int64_t GetInt(wax::StringView section, wax::StringView key, int64_t fallback = 0) const {
            auto* v = GetValue(section, key);
            if (!v || v->m_type != HiveValue::Type::INT)
                return fallback;
            return v->AsInt();
        }

        [[nodiscard]] double GetFloat(wax::StringView section, wax::StringView key, double fallback = 0.0) const {
            auto* v = GetValue(section, key);
            if (!v || v->m_type != HiveValue::Type::FLOAT)
                return fallback;
            return v->AsFloat();
        }

        // -- Iteration --

        using DocumentMap = wax::HashMap<wax::String, SectionMap>;

        [[nodiscard]] const DocumentMap& Sections() const noexcept { return m_sections; }
        [[nodiscard]] DocumentMap& Sections() noexcept { return m_sections; }

        [[nodiscard]] comb::DefaultAllocator& GetAllocator() const noexcept { return *m_alloc; }

    private:
        comb::DefaultAllocator* m_alloc;
        DocumentMap m_sections;
    };
} // namespace nectar
