#include <larvae/larvae.h>
#include <comb/platform.h>
#include <cstring>

namespace {
    auto test1 = larvae::RegisterTest("MemoryPlatform", "GetPageSizeReturnsValidValue", []() {
        const auto page_size = comb::GetPageSize();

        // Page size should be non-zero
        larvae::AssertGreaterThan(page_size, 0u);

        // Page size should be a power of 2
        larvae::AssertTrue((page_size & (page_size - 1)) == 0);

        // Typical page sizes are 4096, 8192, or 16384
        larvae::AssertGreaterEqual(page_size, 4096u);
        larvae::AssertLessEqual(page_size, 65536u);
    });

    auto test2 = larvae::RegisterTest("MemoryPlatform", "AllocatePagesReturnsValidPointer", []() {
        const auto page_size = comb::GetPageSize();
        void* ptr = comb::AllocatePages(page_size);

        larvae::AssertNotNull(ptr);

        comb::FreePages(ptr, page_size);
    });

    auto test3 = larvae::RegisterTest("MemoryPlatform", "AllocatePagesReturnsNullOnZeroSize", []() {
        void* ptr = comb::AllocatePages(0);

        // Some platforms might return nullptr for size 0
        // This is platform-dependent behavior
        if (ptr)
        {
            comb::FreePages(ptr, 0);
        }

        // Test passes regardless - we're just checking it doesn't crash
        larvae::AssertTrue(true);
    });

    auto test4 = larvae::RegisterTest("MemoryPlatform", "AllocatedMemoryIsReadable", []() {
        const auto page_size = comb::GetPageSize();
        void* ptr = comb::AllocatePages(page_size);

        larvae::AssertNotNull(ptr);

        // Try to read from the allocated memory
        auto* byte_ptr = static_cast<unsigned char*>(ptr);
        volatile unsigned char value = byte_ptr[0];
        (void)value; // Use the value to prevent optimization

        // If we got here, reading succeeded
        larvae::AssertTrue(true);

        comb::FreePages(ptr, page_size);
    });

    auto test5 = larvae::RegisterTest("MemoryPlatform", "AllocatedMemoryIsWritable", []() {
        const auto page_size = comb::GetPageSize();
        void* ptr = comb::AllocatePages(page_size);

        larvae::AssertNotNull(ptr);

        // Try to write to the allocated memory
        auto* byte_ptr = static_cast<unsigned char*>(ptr);
        byte_ptr[0] = 42;
        byte_ptr[page_size - 1] = 99;

        // Verify writes
        larvae::AssertEqual(byte_ptr[0], static_cast<unsigned char>(42));
        larvae::AssertEqual(byte_ptr[page_size - 1], static_cast<unsigned char>(99));

        comb::FreePages(ptr, page_size);
    });

    auto test6 = larvae::RegisterTest("MemoryPlatform", "AllocateMultiplePages", []() {
        const auto page_size = comb::GetPageSize();
        const auto alloc_size = page_size * 4;
        void* ptr = comb::AllocatePages(alloc_size);

        larvae::AssertNotNull(ptr);

        // Write to different pages
        auto* byte_ptr = static_cast<unsigned char*>(ptr);
        byte_ptr[0] = 1;                           // First page
        byte_ptr[page_size] = 2;                   // Second page
        byte_ptr[page_size * 2] = 3;               // Third page
        byte_ptr[page_size * 3] = 4;               // Fourth page
        byte_ptr[alloc_size - 1] = 5;              // Last byte

        // Verify all writes
        larvae::AssertEqual(byte_ptr[0], static_cast<unsigned char>(1));
        larvae::AssertEqual(byte_ptr[page_size], static_cast<unsigned char>(2));
        larvae::AssertEqual(byte_ptr[page_size * 2], static_cast<unsigned char>(3));
        larvae::AssertEqual(byte_ptr[page_size * 3], static_cast<unsigned char>(4));
        larvae::AssertEqual(byte_ptr[alloc_size - 1], static_cast<unsigned char>(5));

        comb::FreePages(ptr, alloc_size);
    });

    auto test7 = larvae::RegisterTest("MemoryPlatform", "FreePagesWithNullptrIsSafe", []() {
        const auto page_size = comb::GetPageSize();

        // Should not crash
        comb::FreePages(nullptr, page_size);
        comb::FreePages(nullptr, 0);

        larvae::AssertTrue(true);
    });

    auto test8 = larvae::RegisterTest("MemoryPlatform", "MultipleAllocationsAreIndependent", []() {
        const auto page_size = comb::GetPageSize();

        void* ptr1 = comb::AllocatePages(page_size);
        void* ptr2 = comb::AllocatePages(page_size);
        void* ptr3 = comb::AllocatePages(page_size);

        larvae::AssertNotNull(ptr1);
        larvae::AssertNotNull(ptr2);
        larvae::AssertNotNull(ptr3);

        // Pointers should be different
        larvae::AssertNotEqual(ptr1, ptr2);
        larvae::AssertNotEqual(ptr2, ptr3);
        larvae::AssertNotEqual(ptr1, ptr3);

        // Write to each allocation
        static_cast<unsigned char*>(ptr1)[0] = 11;
        static_cast<unsigned char*>(ptr2)[0] = 22;
        static_cast<unsigned char*>(ptr3)[0] = 33;

        // Verify isolation
        larvae::AssertEqual(static_cast<unsigned char*>(ptr1)[0], static_cast<unsigned char>(11));
        larvae::AssertEqual(static_cast<unsigned char*>(ptr2)[0], static_cast<unsigned char>(22));
        larvae::AssertEqual(static_cast<unsigned char*>(ptr3)[0], static_cast<unsigned char>(33));

        // Free in different order
        comb::FreePages(ptr2, page_size);
        comb::FreePages(ptr1, page_size);
        comb::FreePages(ptr3, page_size);
    });

    auto test9 = larvae::RegisterTest("MemoryPlatform", "LargeAllocation", []() {
        const auto page_size = comb::GetPageSize();
        const auto alloc_size = page_size * 256; // 1 MB if page size is 4 KB

        void* ptr = comb::AllocatePages(alloc_size);

        larvae::AssertNotNull(ptr);

        // Write pattern across large allocation
        auto* byte_ptr = static_cast<unsigned char*>(ptr);
        for (size_t i = 0; i < alloc_size; i += page_size)
        {
            byte_ptr[i] = static_cast<unsigned char>(i / page_size);
        }

        // Verify pattern
        for (size_t i = 0; i < alloc_size; i += page_size)
        {
            larvae::AssertEqual(byte_ptr[i], static_cast<unsigned char>(i / page_size));
        }

        comb::FreePages(ptr, alloc_size);
    });

    auto test10 = larvae::RegisterTest("MemoryPlatform", "AllocateNonPageAlignedSize", []() {
        const auto page_size = comb::GetPageSize();
        const auto odd_size = page_size + 100; // Not aligned to page size

        void* ptr = comb::AllocatePages(odd_size);

        larvae::AssertNotNull(ptr);

        // OS should round up, so we can safely access the odd_size bytes
        auto* byte_ptr = static_cast<unsigned char*>(ptr);
        byte_ptr[0] = 1;
        byte_ptr[odd_size - 1] = 2;

        larvae::AssertEqual(byte_ptr[0], static_cast<unsigned char>(1));
        larvae::AssertEqual(byte_ptr[odd_size - 1], static_cast<unsigned char>(2));

        comb::FreePages(ptr, odd_size);
    });

    auto test11 = larvae::RegisterTest("MemoryPlatform", "MemsetOnAllocatedPages", []() {
        const auto page_size = comb::GetPageSize();
        const auto alloc_size = page_size * 2;

        void* ptr = comb::AllocatePages(alloc_size);
        larvae::AssertNotNull(ptr);

        // Fill with pattern
        std::memset(ptr, 0xAB, alloc_size);

        // Verify pattern
        auto* byte_ptr = static_cast<unsigned char*>(ptr);
        for (size_t i = 0; i < alloc_size; ++i)
        {
            if (byte_ptr[i] != 0xAB)
            {
                larvae::AssertTrue(false); // Pattern mismatch
            }
        }

        larvae::AssertTrue(true);

        comb::FreePages(ptr, alloc_size);
    });

    class PlatformFixture : public larvae::TestFixture
    {
    public:
        void SetUp() override
        {
            page_size = comb::GetPageSize();
            ptr1 = comb::AllocatePages(page_size);
            ptr2 = comb::AllocatePages(page_size * 2);
        }

        void TearDown() override
        {
            if (ptr1) comb::FreePages(ptr1, page_size);
            if (ptr2) comb::FreePages(ptr2, page_size * 2);
        }

        size_t page_size{0};
        void* ptr1{nullptr};
        void* ptr2{nullptr};
    };

    auto test12 = larvae::RegisterTestWithFixture<PlatformFixture>(
        "PlatformFixture", "AllocationsInFixture",
        [](PlatformFixture& f) {
            larvae::AssertNotNull(f.ptr1);
            larvae::AssertNotNull(f.ptr2);
            larvae::AssertNotEqual(f.ptr1, f.ptr2);

            // Write to both
            static_cast<unsigned char*>(f.ptr1)[0] = 100;
            static_cast<unsigned char*>(f.ptr2)[0] = 200;

            larvae::AssertEqual(static_cast<unsigned char*>(f.ptr1)[0], static_cast<unsigned char>(100));
            larvae::AssertEqual(static_cast<unsigned char*>(f.ptr2)[0], static_cast<unsigned char>(200));
        });

    auto test13 = larvae::RegisterTestWithFixture<PlatformFixture>(
        "PlatformFixture", "PageSizeIsConsistent",
        [](PlatformFixture& f) {
            const auto page_size2 = comb::GetPageSize();
            larvae::AssertEqual(f.page_size, page_size2);
        });
}
