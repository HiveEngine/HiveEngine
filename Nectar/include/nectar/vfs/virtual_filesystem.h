#pragma once

#include <nectar/vfs/mount_source.h>
#include <nectar/vfs/file_info.h>
#include <wax/containers/vector.h>
#include <wax/containers/string.h>
#include <wax/containers/string_view.h>
#include <wax/serialization/byte_buffer.h>
#include <comb/default_allocator.h>

namespace nectar
{
    class VirtualFilesystem
    {
    public:
        explicit VirtualFilesystem(comb::DefaultAllocator& alloc);

        /// Mount a source at a mount point prefix.
        /// Higher priority wins when multiple mounts match.
        /// Empty mount_point = root mount (matches all paths).
        /// Does NOT take ownership of source.
        void Mount(wax::StringView mount_point, MountSource* source, int priority = 0);

        /// Unmount a specific source at a specific mount point.
        void Unmount(wax::StringView mount_point, MountSource* source);

        /// Read a file through the VFS. Returns empty buffer if not found.
        [[nodiscard]] wax::ByteBuffer<> ReadSync(wax::StringView path);

        /// Check if a file exists in any mount.
        [[nodiscard]] bool Exists(wax::StringView path) const;

        /// Get file info from the highest-priority mount that has it.
        [[nodiscard]] FileInfo Stat(wax::StringView path) const;

        /// List directory contents merged from all matching mounts.
        void ListDirectory(wax::StringView path, wax::Vector<DirectoryEntry>& out) const;

        /// Number of active mount entries.
        [[nodiscard]] size_t MountCount() const noexcept;

    private:
        struct MountEntry
        {
            wax::String<> prefix;   // normalized mount point
            MountSource* source;
            int priority;
        };

        /// Resolve path to a mount source + relative path.
        /// Returns nullptr if no mount matches.
        MountSource* Resolve(wax::StringView normalized_path,
                             wax::String<>& out_relative) const;

        comb::DefaultAllocator* alloc_;
        wax::Vector<MountEntry> mounts_;  // sorted by priority descending
    };
}
