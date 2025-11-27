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
            nullptr,                  // Let system choose address
            size,                     // Size in bytes
            MEM_RESERVE | MEM_COMMIT, // Reserve and commit pages
            PAGE_READWRITE            // Read/write access
        );
        return ptr;
#elif defined(__unix__) || defined(__APPLE__)
        void* ptr = mmap(
            nullptr,               // Let kernel choose address
            size,                  // Size in bytes
            PROT_READ | PROT_WRITE,// Read/write permissions
            MAP_PRIVATE | MAP_ANONYMOUS, // Private, not backed by file
            -1,                    // No file descriptor (anonymous)
            0                      // No offset (anonymous)
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
            ptr,         // Address to free
            0,           // Must be 0 with MEM_RELEASE
            MEM_RELEASE  // Release reserved pages
        );
#elif defined(__unix__) || defined(__APPLE__)
        munmap(
            ptr,  // Address to unmap
            size  // Size in bytes
        );
#else
        #error "Unsupported platform"
#endif
    }
}
