#pragma once

#include <hive/hive_config.h>

#include <wax/containers/string_view.h>
#include <wax/serialization/byte_span.h>

#include <cstddef>
#include <cstdint>

namespace nectar
{
    /// Cross-platform read-only memory-mapped file.
    /// On Windows uses CreateFileW + CreateFileMappingW + MapViewOfFile.
    /// On Linux/macOS uses open + fstat + mmap.
    class HIVE_API MappedFile
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

        void* m_data{nullptr};
        size_t m_size{0};
#ifdef _WIN32
        void* m_fileHandle{nullptr};
        void* m_mappingHandle{nullptr};
#else
        int fd_{-1};
#endif
    };
} // namespace nectar
