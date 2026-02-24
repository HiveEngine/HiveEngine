#include <nectar/project/project_file.h>

#include <cstdio>
#include <cstring>

namespace nectar
{

ProjectFile::ProjectFile(comb::DefaultAllocator& alloc)
    : alloc_{&alloc}
    , doc_{alloc}
{}

ProjectFile::LoadResult ProjectFile::Load(wax::StringView content)
{
    auto parse_result = HiveParser::Parse(content, *alloc_);

    LoadResult result{};
    result.errors = static_cast<wax::Vector<HiveParseError>&&>(parse_result.errors);

    if (!result.errors.IsEmpty())
        return result;

    doc_ = static_cast<HiveDocument&&>(parse_result.document);

    if (!Validate(result.errors))
        return result;

    result.success = true;
    return result;
}

ProjectFile::LoadResult ProjectFile::LoadFromDisk(wax::StringView file_path)
{
    LoadResult result{};

    wax::String<> path{*alloc_, file_path};
    FILE* file = std::fopen(path.CStr(), "rb");
    if (!file)
    {
        HiveParseError err{};
        err.line = 0;
        err.message = wax::String<>{*alloc_, "Failed to open file"};
        result.errors.PushBack(static_cast<HiveParseError&&>(err));
        return result;
    }

    std::fseek(file, 0, SEEK_END);
    long file_size = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);

    if (file_size <= 0)
    {
        std::fclose(file);
        HiveParseError err{};
        err.line = 0;
        err.message = wax::String<>{*alloc_, "File is empty"};
        result.errors.PushBack(static_cast<HiveParseError&&>(err));
        return result;
    }

    wax::String<> content{*alloc_};
    content.Reserve(static_cast<size_t>(file_size));
    // Read into a temporary buffer, then append
    char buf[4096];
    size_t remaining = static_cast<size_t>(file_size);
    while (remaining > 0)
    {
        size_t to_read = remaining < sizeof(buf) ? remaining : sizeof(buf);
        size_t read = std::fread(buf, 1, to_read, file);
        if (read == 0) break;
        content.Append(buf, read);
        remaining -= read;
    }
    std::fclose(file);

    return Load(content.View());
}

void ProjectFile::Create(const ProjectDesc& desc)
{
    doc_ = HiveDocument{*alloc_};

    doc_.SetValue("project", "name", HiveValue::MakeString(*alloc_, desc.name));

    if (!desc.version.IsEmpty())
        doc_.SetValue("project", "version", HiveValue::MakeString(*alloc_, desc.version));

    if (!desc.engine_path.IsEmpty())
        doc_.SetValue("engine", "path", HiveValue::MakeString(*alloc_, desc.engine_path));

    if (!desc.backend.IsEmpty())
        doc_.SetValue("render", "backend", HiveValue::MakeString(*alloc_, desc.backend));
}

wax::String<> ProjectFile::Serialize() const
{
    return HiveWriter::Write(doc_, *alloc_);
}

bool ProjectFile::SaveToDisk(wax::StringView file_path) const
{
    wax::String<> content = Serialize();
    wax::String<> path{*alloc_, file_path};

    FILE* file = std::fopen(path.CStr(), "wb");
    if (!file)
        return false;

    if (content.Size() > 0)
        std::fwrite(content.CStr(), 1, content.Size(), file);

    std::fclose(file);
    return true;
}

wax::StringView ProjectFile::Name() const
{
    return doc_.GetString("project", "name");
}

wax::StringView ProjectFile::Version() const
{
    return doc_.GetString("project", "version");
}

wax::StringView ProjectFile::EnginePath() const
{
    return doc_.GetString("engine", "path");
}

wax::StringView ProjectFile::Backend() const
{
    return doc_.GetString("render", "backend");
}

wax::StringView ProjectFile::AssetsRelative() const
{
    return doc_.GetString("paths", "assets", "assets");
}

wax::StringView ProjectFile::CacheRelative() const
{
    return doc_.GetString("paths", "cache", ".hive-cache");
}

wax::StringView ProjectFile::SourceRelative() const
{
    return doc_.GetString("paths", "source", "src");
}

static void NormalizeSeparators(wax::String<>& path)
{
    char* data = path.Data();
    for (size_t i = 0; i < path.Size(); ++i)
    {
        if (data[i] == '\\')
            data[i] = '/';
    }
}

static wax::String<> JoinPath(comb::DefaultAllocator& alloc, wax::StringView root, wax::StringView relative)
{
    wax::String<> result{alloc, root};

    if (!root.IsEmpty() && root.Data()[root.Size() - 1] != '/' && root.Data()[root.Size() - 1] != '\\')
        result.Append('/');

    result.Append(relative);
    NormalizeSeparators(result);
    return result;
}

ProjectPaths ProjectFile::ResolvePaths(wax::StringView project_root) const
{
    ProjectPaths paths{};

    paths.root = wax::String<>{*alloc_, project_root};
    NormalizeSeparators(paths.root);

    paths.assets = JoinPath(*alloc_, project_root, AssetsRelative());
    paths.cache = JoinPath(*alloc_, project_root, CacheRelative());
    paths.source = JoinPath(*alloc_, project_root, SourceRelative());

    paths.cas = JoinPath(*alloc_, paths.cache.View(), "cas");
    paths.import_cache = JoinPath(*alloc_, paths.cache.View(), "import_cache.bin");

    return paths;
}

bool ProjectFile::Validate(wax::Vector<HiveParseError>& errors) const
{
    wax::StringView name = doc_.GetString("project", "name");
    if (name.IsEmpty())
    {
        HiveParseError err{};
        err.line = 0;
        err.message = wax::String<>{*alloc_, "[project] name is required"};
        errors.PushBack(static_cast<HiveParseError&&>(err));
        return false;
    }
    return true;
}

} // namespace nectar
