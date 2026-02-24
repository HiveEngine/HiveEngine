#include <comb/precomp.h>
#include <comb/platform.h>

// Platform-specific headers
#if defined(_WIN32)
    #include <windows.h>
#elif defined(__unix__) || defined(__APPLE__)
    #include <sys/mman.h>
    #include <unistd.h>
#else
    #error "Unsupported platform"
#endif

namespace comb
{
    size_t GetPageSize()
    {
#if defined(_WIN32)
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        return info.dwPageSize;
#elif defined(__unix__) || defined(__APPLE__)
        return static_cast<size_t>(sysconf(_SC_PAGESIZE));
#else
        #error "Unsupported platform"
#endif
    }

    void* AllocatePages(size_t size)
    {
#if defined(_WIN32)
        void* ptr = VirtualAlloc(
            nullptr,
            size,
            MEM_RESERVE | MEM_COMMIT,
            PAGE_READWRITE
        );
        return ptr;
#elif defined(__unix__) || defined(__APPLE__)
        void* ptr = mmap(
            nullptr,
            size,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS,
            -1,
            0
        );

        if (ptr == MAP_FAILED)
        {
            return nullptr;
        }

        return ptr;
#else
        #error "Unsupported platform"
#endif
    }

    void FreePages(void* ptr, size_t size)
    {
        if (!ptr) return;

#if defined(_WIN32)
        (void)size; // Windows doesn't need size for VirtualFree with MEM_RELEASE
        VirtualFree(
            ptr,
            0,
            MEM_RELEASE
        );
#elif defined(__unix__) || defined(__APPLE__)
        munmap(ptr, size);
#else
        #error "Unsupported platform"
#endif
    }
}
