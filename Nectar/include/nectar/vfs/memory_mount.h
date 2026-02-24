#pragma once

#include <nectar/vfs/mount_source.h>
#include <wax/containers/hash_map.h>
#include <wax/containers/string.h>

namespace nectar
{
    /// In-memory mount source for testing and tools.
    class MemoryMountSource final : public MountSource
    {
    public:
        explicit MemoryMountSource(comb::DefaultAllocator& alloc);

        /// Add or overwrite a file. Copies the data.
        void AddFile(wax::StringView path, wax::ByteSpan data);

        /// Remove a file. Returns true if it existed.
        bool RemoveFile(wax::StringView path);

        /// Number of files stored.
        [[nodiscard]] size_t FileCount() const noexcept;

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
        comb::DefaultAllocator* alloc_;
        wax::HashMap<wax::String<>, wax::Vector<uint8_t>> files_;
    };
}
