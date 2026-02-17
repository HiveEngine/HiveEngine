#pragma once

#include <nectar/vfs/file_info.h>
#include <wax/containers/string_view.h>
#include <wax/containers/vector.h>
#include <wax/serialization/byte_buffer.h>
#include <comb/default_allocator.h>

namespace nectar
{
    /// Abstract interface for a mountable data source.
    /// All paths passed to these methods are already normalized and relative
    /// to the mount point (stripped of the mount prefix).
    class MountSource
    {
    public:
        virtual ~MountSource() = default;

        [[nodiscard]] virtual wax::ByteBuffer<> ReadFile(
            wax::StringView path, comb::DefaultAllocator& alloc) = 0;

        [[nodiscard]] virtual bool Exists(wax::StringView path) const = 0;

        [[nodiscard]] virtual FileInfo Stat(wax::StringView path) const = 0;

        virtual void ListDirectory(
            wax::StringView path,
            wax::Vector<DirectoryEntry>& out,
            comb::DefaultAllocator& alloc) const = 0;
    };
}
