#pragma once

#include <nectar/hive/hive_document.h>
#include <nectar/hive/hive_parser.h>
#include <nectar/hive/hive_writer.h>
#include <wax/containers/string.h>
#include <wax/containers/string_view.h>
#include <wax/containers/vector.h>
#include <comb/default_allocator.h>

namespace nectar
{
    struct ProjectPaths
    {
        wax::String<> root;
        wax::String<> assets;
        wax::String<> cache;
        wax::String<> cas;
        wax::String<> source;
        wax::String<> import_cache;
    };

    struct ProjectDesc
    {
        wax::StringView name;
        wax::StringView version;
        wax::StringView engine_path;
        wax::StringView backend;
    };

    class ProjectFile
    {
    public:
        explicit ProjectFile(comb::DefaultAllocator& alloc);

        struct LoadResult
        {
            bool success{false};
            wax::Vector<HiveParseError> errors;
        };

        [[nodiscard]] LoadResult Load(wax::StringView content);
        [[nodiscard]] LoadResult LoadFromDisk(wax::StringView file_path);

        void Create(const ProjectDesc& desc);

        [[nodiscard]] wax::String<> Serialize() const;
        [[nodiscard]] bool SaveToDisk(wax::StringView file_path) const;

        [[nodiscard]] wax::StringView Name() const;
        [[nodiscard]] wax::StringView Version() const;
        [[nodiscard]] wax::StringView EnginePath() const;
        [[nodiscard]] wax::StringView Backend() const;

        [[nodiscard]] wax::StringView AssetsRelative() const;
        [[nodiscard]] wax::StringView CacheRelative() const;
        [[nodiscard]] wax::StringView SourceRelative() const;

        [[nodiscard]] ProjectPaths ResolvePaths(wax::StringView project_root) const;

        [[nodiscard]] const HiveDocument& Document() const noexcept { return doc_; }
        [[nodiscard]] HiveDocument& Document() noexcept { return doc_; }

    private:
        comb::DefaultAllocator* alloc_;
        HiveDocument doc_;

        [[nodiscard]] bool Validate(wax::Vector<HiveParseError>& errors) const;
    };
}
