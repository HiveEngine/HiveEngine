#pragma once

#include <nectar/vfs/mount_source.h>
#include <wax/containers/string.h>

namespace nectar
{
    /// Mount source backed by loose files on disk, read via memory mapping.
    /// Faster than DiskMountSource for reads (no per-read syscall, OS prefetch).
    class MmapMountSource final : public MountSource
    {
    public:
        MmapMountSource(wax::StringView root_dir, comb::DefaultAllocator& alloc);

        // -- MountSource --
        [[nodiscard]] wax::ByteBuffer<> ReadFile(
            wax::StringView path, comb::DefaultAllocator& alloc) override;
        [[nodiscard]] bool Exists(wax::StringView path) const override;
        [[nodiscard]] FileInfo Stat(wax::StringView path) const override;
        void ListDirectory(
            wax::StringView path,
            wax::Vector<DirectoryEntry>& out,
            comb::DefaultAllocator& alloc) const override;

    private:
        wax::String<> BuildFullPath(wax::StringView relative, comb::DefaultAllocator& alloc) const;

        comb::DefaultAllocator* alloc_;
        wax::String<> root_dir_;
    };
}
