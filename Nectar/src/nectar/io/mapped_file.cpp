#include <nectar/io/mapped_file.h>
#include <hive/profiling/profiler.h>

#ifdef _WIN32
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   include <windows.h>
#else
#   include <sys/mman.h>
#   include <sys/stat.h>
#   include <fcntl.h>
#   include <unistd.h>
#endif

namespace nectar
{
#ifdef _WIN32

    MappedFile MappedFile::Open(wax::StringView path)
    {
        HIVE_PROFILE_SCOPE_N("MappedFile::Open");
        MappedFile result;

        // Convert UTF-8 path to wide string
        // wax::StringView is not null-terminated, so copy to a local buffer
        char narrow[1024];
        size_t len = path.Size() < sizeof(narrow) - 1 ? path.Size() : sizeof(narrow) - 1;
        for (size_t i = 0; i < len; ++i)
            narrow[i] = path.Data()[i];
        narrow[len] = '\0';

        wchar_t wide[1024];
        int wlen = MultiByteToWideChar(CP_UTF8, 0, narrow, static_cast<int>(len), wide, 1024);
        if (wlen <= 0) return result;
        wide[wlen] = L'\0';

        HANDLE file = CreateFileW(wide, GENERIC_READ, FILE_SHARE_READ,
                                   nullptr, OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return result;

        LARGE_INTEGER file_size;
        if (!GetFileSizeEx(file, &file_size) || file_size.QuadPart == 0)
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

        result.data_ = view;
        result.size_ = static_cast<size_t>(file_size.QuadPart);
        result.file_handle_ = file;
        result.mapping_handle_ = mapping;
        return result;
    }

    void MappedFile::Close() noexcept
    {
        if (data_)
            UnmapViewOfFile(data_);
        if (mapping_handle_)
            CloseHandle(mapping_handle_);
        if (file_handle_)
            CloseHandle(file_handle_);
        data_ = nullptr;
        size_ = 0;
        file_handle_ = nullptr;
        mapping_handle_ = nullptr;
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
        if (fd < 0) return result;

        struct stat st;
        if (::fstat(fd, &st) != 0 || st.st_size == 0)
        {
            ::close(fd);
            return result;
        }

        void* addr = ::mmap(nullptr, static_cast<size_t>(st.st_size),
                            PROT_READ, MAP_PRIVATE, fd, 0);
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
        : data_{other.data_}
        , size_{other.size_}
#ifdef _WIN32
        , file_handle_{other.file_handle_}
        , mapping_handle_{other.mapping_handle_}
#else
        , fd_{other.fd_}
#endif
    {
        other.data_ = nullptr;
        other.size_ = 0;
#ifdef _WIN32
        other.file_handle_ = nullptr;
        other.mapping_handle_ = nullptr;
#else
        other.fd_ = -1;
#endif
    }

    MappedFile& MappedFile::operator=(MappedFile&& other) noexcept
    {
        if (this != &other)
        {
            Close();
            data_ = other.data_;
            size_ = other.size_;
#ifdef _WIN32
            file_handle_ = other.file_handle_;
            mapping_handle_ = other.mapping_handle_;
            other.file_handle_ = nullptr;
            other.mapping_handle_ = nullptr;
#else
            fd_ = other.fd_;
            other.fd_ = -1;
#endif
            other.data_ = nullptr;
            other.size_ = 0;
        }
        return *this;
    }

    bool MappedFile::IsValid() const noexcept
    {
        return data_ != nullptr;
    }

    const uint8_t* MappedFile::Data() const noexcept
    {
        return static_cast<const uint8_t*>(data_);
    }

    size_t MappedFile::Size() const noexcept
    {
        return size_;
    }

    wax::ByteSpan MappedFile::View() const noexcept
    {
        return wax::ByteSpan{static_cast<const uint8_t*>(data_), size_};
    }
}
