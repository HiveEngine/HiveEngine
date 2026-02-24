#pragma once

#include <wax/serialization/byte_span.h>
#include <wax/containers/string_view.h>
#include <cstddef>
#include <cstdint>

namespace nectar
{
    /// Cross-platform read-only memory-mapped file.
    /// On Windows uses CreateFileW + CreateFileMappingW + MapViewOfFile.
    /// On Linux/macOS uses open + fstat + mmap.
    class MappedFile
    {
    public:
        /// Open a file for memory mapping. Returns invalid MappedFile on failure.
        [[nodiscard]] static MappedFile Open(wax::StringView path);

        MappedFile() = default;
        ~MappedFile();

        MappedFile(MappedFile&& other) noexcept;
        MappedFile& operator=(MappedFile&& other) noexcept;

        MappedFile(const MappedFile&) = delete;
        MappedFile& operator=(const MappedFile&) = delete;

        [[nodiscard]] bool IsValid() const noexcept;
        [[nodiscard]] const uint8_t* Data() const noexcept;
        [[nodiscard]] size_t Size() const noexcept;
        [[nodiscard]] wax::ByteSpan View() const noexcept;

    private:
        void Close() noexcept;

        void* data_{nullptr};
        size_t size_{0};
#ifdef _WIN32
        void* file_handle_{nullptr};
        void* mapping_handle_{nullptr};
#else
        int fd_{-1};
#endif
    };
}
