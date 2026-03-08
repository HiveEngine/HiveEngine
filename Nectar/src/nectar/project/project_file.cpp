#include <nectar/project/project_file.h>

#include <cstdio>

namespace nectar
{
    ProjectFile::ProjectFile(comb::DefaultAllocator& alloc)
        : m_alloc{&alloc}
        , m_doc{alloc}
    {
    }

    ProjectFile::LoadResult ProjectFile::Load(wax::StringView content)
    {
        auto parseResult = HiveParser::Parse(content, *m_alloc);

        LoadResult result{};
        result.m_errors = static_cast<wax::Vector<HiveParseError>&&>(parseResult.m_errors);

        if (!result.m_errors.IsEmpty())
        {
            return result;
        }

        m_doc = static_cast<HiveDocument&&>(parseResult.m_document);

        if (!Validate(result.m_errors))
        {
            return result;
        }

        result.m_success = true;
        return result;
    }

    ProjectFile::LoadResult ProjectFile::LoadFromDisk(wax::StringView filePath)
    {
        LoadResult result{};

        wax::String path{*m_alloc, filePath};
        FILE* file = std::fopen(path.CStr(), "rb");
        if (!file)
        {
            HiveParseError err{};
            err.m_line = 0;
            err.m_message = wax::String{*m_alloc, "Failed to open file"};
            result.m_errors.PushBack(static_cast<HiveParseError&&>(err));
            return result;
        }

        std::fseek(file, 0, SEEK_END);
        long fileSize = std::ftell(file);
        std::fseek(file, 0, SEEK_SET);

        if (fileSize <= 0)
        {
            std::fclose(file);
            HiveParseError err{};
            err.m_line = 0;
            err.m_message = wax::String{*m_alloc, "File is empty"};
            result.m_errors.PushBack(static_cast<HiveParseError&&>(err));
            return result;
        }

        wax::String content{*m_alloc};
        content.Reserve(static_cast<size_t>(fileSize));

        char buffer[4096];
        size_t remaining = static_cast<size_t>(fileSize);
        while (remaining > 0)
        {
            size_t toRead = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
            size_t read = std::fread(buffer, 1, toRead, file);
            if (read == 0)
            {
                break;
            }
            content.Append(buffer, read);
            remaining -= read;
        }
        std::fclose(file);

        return Load(content.View());
    }

    void ProjectFile::Create(const ProjectDesc& desc)
    {
        m_doc = HiveDocument{*m_alloc};

        m_doc.SetValue("project", "name", HiveValue::MakeString(*m_alloc, desc.m_name));

        if (!desc.m_version.IsEmpty())
        {
            m_doc.SetValue("project", "version", HiveValue::MakeString(*m_alloc, desc.m_version));
        }

        if (!desc.m_enginePath.IsEmpty())
        {
            m_doc.SetValue("engine", "path", HiveValue::MakeString(*m_alloc, desc.m_enginePath));
        }

        if (!desc.m_backend.IsEmpty())
        {
            m_doc.SetValue("render", "backend", HiveValue::MakeString(*m_alloc, desc.m_backend));
        }
    }

    wax::String ProjectFile::Serialize() const
    {
        return HiveWriter::Write(m_doc, *m_alloc);
    }

    bool ProjectFile::SaveToDisk(wax::StringView filePath) const
    {
        wax::String content = Serialize();
        wax::String path{*m_alloc, filePath};

        FILE* file = std::fopen(path.CStr(), "wb");
        if (!file)
        {
            return false;
        }

        if (content.Size() > 0)
        {
            std::fwrite(content.CStr(), 1, content.Size(), file);
        }

        std::fclose(file);
        return true;
    }

    wax::StringView ProjectFile::Name() const
    {
        return m_doc.GetString("project", "name");
    }

    wax::StringView ProjectFile::Version() const
    {
        return m_doc.GetString("project", "version");
    }

    wax::StringView ProjectFile::EnginePath() const
    {
        return m_doc.GetString("engine", "path");
    }

    wax::StringView ProjectFile::Backend() const
    {
        return m_doc.GetString("render", "backend");
    }

    wax::StringView ProjectFile::AssetsRelative() const
    {
        return m_doc.GetString("paths", "assets", "assets");
    }

    wax::StringView ProjectFile::CacheRelative() const
    {
        return m_doc.GetString("paths", "cache", ".hive-cache");
    }

    wax::StringView ProjectFile::SourceRelative() const
    {
        return m_doc.GetString("paths", "source", "src");
    }

    static void NormalizeSeparators(wax::String& path)
    {
        char* data = path.Data();
        for (size_t i = 0; i < path.Size(); ++i)
        {
            if (data[i] == '\\')
            {
                data[i] = '/';
            }
        }
    }

    static wax::String JoinPath(comb::DefaultAllocator& alloc, wax::StringView root, wax::StringView relative)
    {
        wax::String result{alloc, root};

        if (!root.IsEmpty() && root.Data()[root.Size() - 1] != '/' && root.Data()[root.Size() - 1] != '\\')
        {
            result.Append('/');
        }

        result.Append(relative);
        NormalizeSeparators(result);
        return result;
    }

    ProjectPaths ProjectFile::ResolvePaths(wax::StringView projectRoot) const
    {
        ProjectPaths paths{};

        paths.m_root = wax::String{*m_alloc, projectRoot};
        NormalizeSeparators(paths.m_root);

        paths.m_assets = JoinPath(*m_alloc, projectRoot, AssetsRelative());
        paths.m_cache = JoinPath(*m_alloc, projectRoot, CacheRelative());
        paths.m_source = JoinPath(*m_alloc, projectRoot, SourceRelative());

        paths.m_cas = JoinPath(*m_alloc, paths.m_cache.View(), "cas");
        paths.m_importCache = JoinPath(*m_alloc, paths.m_cache.View(), "import_cache.bin");

        return paths;
    }

    bool ProjectFile::Validate(wax::Vector<HiveParseError>& errors) const
    {
        wax::StringView name = m_doc.GetString("project", "name");
        if (name.IsEmpty())
        {
            HiveParseError err{};
            err.m_line = 0;
            err.m_message = wax::String{*m_alloc, "[project] name is required"};
            errors.PushBack(static_cast<HiveParseError&&>(err));
            return false;
        }
        return true;
    }
} // namespace nectar
