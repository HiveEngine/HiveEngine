#pragma once

#include <hive/hive_config.h>

#include <nectar/pak/pak_reader.h>
#include <nectar/vfs/mount_source.h>

namespace nectar
{
    /// Mount source backed by a .npak archive.
    /// Resolves VFS paths via the embedded AssetManifest.
    /// Takes ownership of the PakReader.
    class HIVE_API PakMountSource final : public MountSource
    {
    public:
        PakMountSource(PakReader* reader, comb::DefaultAllocator& alloc);
        ~PakMountSource() override;

        // Non-copyable
        PakMountSource(const PakMountSource&) = delete;
        PakMountSource& operator=(const PakMountSource&) = delete;

        // -- MountSource --
        [[nodiscard]] wax::ByteBuffer ReadFile(wax::StringView path, comb::DefaultAllocator& alloc) override;
        [[nodiscard]] bool Exists(wax::StringView path) const override;
        [[nodiscard]] FileInfo Stat(wax::StringView path) const override;
        void ListDirectory(wax::StringView path, wax::Vector<DirectoryEntry>& out,
                           comb::DefaultAllocator& alloc) const override;

    private:
        comb::DefaultAllocator* m_alloc;
        PakReader* m_reader; // owned
    };
} // namespace nectar
