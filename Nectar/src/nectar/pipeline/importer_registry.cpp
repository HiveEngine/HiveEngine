#include <nectar/pipeline/importer_registry.h>
#include <nectar/vfs/path.h>

namespace nectar
{
    ImporterRegistry::ImporterRegistry(comb::DefaultAllocator& alloc)
        : m_alloc{&alloc}
        , m_extensionMap{alloc, 32}
    {
    }

    void ImporterRegistry::Register(IAssetImporter* importer)
    {
        if (!importer)
            return;

        auto exts = importer->SourceExtensions();
        for (size_t i = 0; i < exts.Size(); ++i)
        {
            wax::StringView raw{exts[i]};
            wax::String key{*m_alloc};
            for (size_t c = 0; c < raw.Size(); ++c)
            {
                char ch = raw[c];
                if (ch >= 'A' && ch <= 'Z')
                    ch = static_cast<char>(ch + ('a' - 'A'));
                key.Append(ch);
            }

            // Overwrite if already registered
            auto* existing = m_extensionMap.Find(key);
            if (existing)
                *existing = importer;
            else
                m_extensionMap.Insert(static_cast<wax::String&&>(key), importer);
        }
    }

    IAssetImporter* ImporterRegistry::FindByExtension(wax::StringView extension) const
    {
        wax::String key{*m_alloc};
        for (size_t i = 0; i < extension.Size(); ++i)
        {
            char ch = extension[i];
            if (ch >= 'A' && ch <= 'Z')
                ch = static_cast<char>(ch + ('a' - 'A'));
            key.Append(ch);
        }

        auto* found = m_extensionMap.Find(key);
        return found ? *found : nullptr;
    }

    IAssetImporter* ImporterRegistry::FindByPath(wax::StringView path) const
    {
        if (path.Size() == 0)
            return nullptr;
        auto ext = PathExtension(path);
        if (ext.Size() == 0)
            return nullptr;
        return FindByExtension(ext);
    }

    size_t ImporterRegistry::Count() const noexcept
    {
        return m_extensionMap.Count();
    }
} // namespace nectar
