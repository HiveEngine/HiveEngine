#pragma once

#include <comb/default_allocator.h>

#include <wax/containers/string.h>
#include <wax/containers/string_view.h>
#include <wax/containers/vector.h>
#include <wax/serialization/byte_buffer.h>

#include <nectar/vfs/file_info.h>
#include <nectar/vfs/mount_source.h>

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
        void Mount(wax::StringView mountPoint, MountSource* source, int priority = 0);

        /// Unmount a specific source at a specific mount point.
        void Unmount(wax::StringView mountPoint, MountSource* source);

        /// Read a file through the VFS. Returns empty buffer if not found.
        [[nodiscard]] wax::ByteBuffer ReadSync(wax::StringView path);

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
            wax::String m_prefix; // normalized mount point
            MountSource* m_source;
            int m_priority;
        };

        /// Resolve path to a mount source + relative path.
        /// Returns nullptr if no mount matches.
        MountSource* Resolve(wax::StringView normalizedPath, wax::String& outRelative) const;

        comb::DefaultAllocator* m_alloc;
        wax::Vector<MountEntry> m_mounts; // sorted by priority descending
    };
} // namespace nectar
