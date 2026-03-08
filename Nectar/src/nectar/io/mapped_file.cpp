#include <hive/profiling/profiler.h>

#include <nectar/io/mapped_file.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace nectar
{
#ifdef _WIN32

    MappedFile MappedFile::Open(wax::StringView path)
    {
        HIVE_PROFILE_SCOPE_N("MappedFile::Open");
        MappedFile result;

        // wax::StringView is not null-terminated, so copy to a local buffer
        char narrow[1024];
        size_t len = path.Size() < sizeof(narrow) - 1 ? path.Size() : sizeof(narrow) - 1;
        for (size_t i = 0; i < len; ++i)
            narrow[i] = path.Data()[i];
        narrow[len] = '\0';

        wchar_t wide[1024];
        int wlen = MultiByteToWideChar(CP_UTF8, 0, narrow, static_cast<int>(len), wide, 1024);
        if (wlen <= 0)
            return result;
        wide[wlen] = L'\0';

        HANDLE file =
            CreateFileW(wide, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return result;

        LARGE_INTEGER fileSize;
        if (!GetFileSizeEx(file, &fileSize) || fileSize.QuadPart == 0)
        {
            CloseHandle(file);
            return result;
        }

        HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (!mapping)
        {
            CloseHandle(file);
            return result;
        }

        void* view = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
        if (!view)
        {
            CloseHandle(mapping);
            CloseHandle(file);
            return result;
        }

        result.m_data = view;
        result.m_size = static_cast<size_t>(fileSize.QuadPart);
        result.m_fileHandle = file;
        result.m_mappingHandle = mapping;
        return result;
    }

    void MappedFile::Close() noexcept
    {
        if (m_data)
            UnmapViewOfFile(m_data);
        if (m_mappingHandle)
            CloseHandle(m_mappingHandle);
        if (m_fileHandle)
            CloseHandle(m_fileHandle);
        m_data = nullptr;
        m_size = 0;
        m_fileHandle = nullptr;
        m_mappingHandle = nullptr;
    }

#else // POSIX

    MappedFile MappedFile::Open(wax::StringView path)
    {
        HIVE_PROFILE_SCOPE_N("MappedFile::Open");
        MappedFile result;

        // Need null-terminated string
        char buf[1024];
        size_t len = path.Size() < sizeof(buf) - 1 ? path.Size() : sizeof(buf) - 1;
        for (size_t i = 0; i < len; ++i)
            buf[i] = path.Data()[i];
        buf[len] = '\0';

        int fd = ::open(buf, O_RDONLY);
        if (fd < 0)
            return result;

        struct stat st;
        if (::fstat(fd, &st) != 0 || st.st_size == 0)
        {
            ::close(fd);
            return result;
        }

        void* addr = ::mmap(nullptr, static_cast<size_t>(st.st_size), PROT_READ, MAP_PRIVATE, fd, 0);
        if (addr == MAP_FAILED)
        {
            ::close(fd);
            return result;
        }

        result.data_ = addr;
        result.size_ = static_cast<size_t>(st.st_size);
        result.fd_ = fd;
        return result;
    }

    void MappedFile::Close() noexcept
    {
        if (data_)
            ::munmap(data_, size_);
        if (fd_ >= 0)
            ::close(fd_);
        data_ = nullptr;
        size_ = 0;
        fd_ = -1;
    }

#endif // _WIN32

    MappedFile::~MappedFile()
    {
        Close();
    }

    MappedFile::MappedFile(MappedFile&& other) noexcept
        : m_data{other.m_data}
        , m_size{other.m_size}
#ifdef _WIN32
        , m_fileHandle{other.m_fileHandle}
        , m_mappingHandle{other.m_mappingHandle}
#else
        , fd_{other.fd_}
#endif
    {
        other.m_data = nullptr;
        other.m_size = 0;
#ifdef _WIN32
        other.m_fileHandle = nullptr;
        other.m_mappingHandle = nullptr;
#else
        other.fd_ = -1;
#endif
    }

    MappedFile& MappedFile::operator=(MappedFile&& other) noexcept
    {
        if (this != &other)
        {
            Close();
            m_data = other.m_data;
            m_size = other.m_size;
#ifdef _WIN32
            m_fileHandle = other.m_fileHandle;
            m_mappingHandle = other.m_mappingHandle;
            other.m_fileHandle = nullptr;
            other.m_mappingHandle = nullptr;
#else
            fd_ = other.fd_;
            other.fd_ = -1;
#endif
            other.m_data = nullptr;
            other.m_size = 0;
        }
        return *this;
    }

    bool MappedFile::IsValid() const noexcept
    {
        return m_data != nullptr;
    }

    const uint8_t* MappedFile::Data() const noexcept
    {
        return static_cast<const uint8_t*>(m_data);
    }

    size_t MappedFile::Size() const noexcept
    {
        return m_size;
    }

    wax::ByteSpan MappedFile::View() const noexcept
    {
        return wax::ByteSpan{static_cast<const uint8_t*>(m_data), m_size};
    }
} // namespace nectar
