#pragma once

#include <hive/hive_config.h>

#include <comb/default_allocator.h>

#include <wax/containers/string.h>
#include <wax/containers/string_view.h>
#include <wax/containers/vector.h>

#include <nectar/hive/hive_document.h>
#include <nectar/hive/hive_parser.h>
#include <nectar/hive/hive_writer.h>

namespace nectar
{
    struct ProjectPaths
    {
        wax::String m_root;
        wax::String m_assets;
        wax::String m_startupScene;
        wax::String m_cache;
        wax::String m_cas;
        wax::String m_source;
        wax::String m_importCache;
    };

    struct ProjectDesc
    {
        wax::StringView m_name;
        wax::StringView m_version;
        wax::StringView m_enginePath;
        wax::StringView m_backend;
        wax::StringView m_startupScene;
    };

    class HIVE_API ProjectFile
    {
    public:
        explicit ProjectFile(comb::DefaultAllocator& alloc);

        struct LoadResult
        {
            bool m_success{false};
            wax::Vector<HiveParseError> m_errors;
        };

        [[nodiscard]] LoadResult Load(wax::StringView content);
        [[nodiscard]] LoadResult LoadFromDisk(wax::StringView filePath);

        void Create(const ProjectDesc& desc);

        [[nodiscard]] wax::String Serialize() const;
        [[nodiscard]] bool SaveToDisk(wax::StringView filePath) const;

        [[nodiscard]] wax::StringView Name() const;
        [[nodiscard]] wax::StringView Version() const;
        [[nodiscard]] wax::StringView EnginePath() const;
        [[nodiscard]] wax::StringView Backend() const;

        [[nodiscard]] wax::StringView AssetsRelative() const;
        [[nodiscard]] wax::StringView StartupSceneRelative() const;
        [[nodiscard]] wax::StringView CacheRelative() const;
        [[nodiscard]] wax::StringView SourceRelative() const;

        [[nodiscard]] ProjectPaths ResolvePaths(wax::StringView projectRoot) const;

        [[nodiscard]] const HiveDocument& Document() const noexcept
        {
            return m_doc;
        }
        [[nodiscard]] HiveDocument& Document() noexcept
        {
            return m_doc;
        }

    private:
        comb::DefaultAllocator* m_alloc;
        HiveDocument m_doc;

        [[nodiscard]] bool Validate(wax::Vector<HiveParseError>& errors) const;
    };
} // namespace nectar
