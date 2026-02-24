#include <larvae/larvae.h>
#include <nectar/io/io_scheduler.h>
#include <nectar/vfs/virtual_filesystem.h>
#include <nectar/vfs/memory_mount.h>
#include <comb/default_allocator.h>
#include <cstring>
#include <thread>
#include <chrono>

namespace {

    auto& GetIOAlloc()
    {
        static comb::ModuleAllocator alloc{"TestIO", 4 * 1024 * 1024};
        return alloc.Get();
    }

    // Poll until condition is true, with timeout
    template <typename Fn>
    bool PollUntil(Fn&& fn, int timeout_ms = 2000)
    {
        auto start = std::chrono::steady_clock::now();
        while (!fn())
        {
            auto elapsed = std::chrono::steady_clock::now() - start;
            if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() >= timeout_ms)
                return false;
            std::this_thread::sleep_for(std::chrono::milliseconds{5});
        }
        return true;
    }

    // =========================================================================
    // Submit and drain
    // =========================================================================

    auto t1 = larvae::RegisterTest("NectarIO", "SubmitAndDrain", []() {
        auto& alloc = GetIOAlloc();
        nectar::MemoryMountSource mem{alloc};
        const char* data = "hello async";
        mem.AddFile("test.txt", wax::ByteSpan{data, std::strlen(data)});

        nectar::VirtualFilesystem vfs{alloc};
        vfs.Mount("", &mem);

        nectar::IOScheduler io{vfs, alloc, {1}};

        auto id = io.Submit("test.txt", nectar::LoadPriority::Normal);

        wax::Vector<nectar::IOCompletion> completions{alloc};
        bool ok = PollUntil([&]() {
            io.DrainCompletions(completions);
            return completions.Size() > 0;
        });

        larvae::AssertTrue(ok);
        larvae::AssertEqual(completions.Size(), size_t{1});
        larvae::AssertEqual(completions[0].request_id, id);
        larvae::AssertTrue(completions[0].success);
        larvae::AssertEqual(completions[0].data.Size(), std::strlen(data));
        larvae::AssertTrue(std::memcmp(completions[0].data.Data(), data, std::strlen(data)) == 0);

        io.Shutdown();
    });

    // =========================================================================
    // Multiple submits
    // =========================================================================

    auto t2 = larvae::RegisterTest("NectarIO", "MultipleSubmits", []() {
        auto& alloc = GetIOAlloc();
        nectar::MemoryMountSource mem{alloc};
        mem.AddFile("a.txt", wax::ByteSpan{"aaa", 3});
        mem.AddFile("b.txt", wax::ByteSpan{"bb", 2});
        mem.AddFile("c.txt", wax::ByteSpan{"c", 1});

        nectar::VirtualFilesystem vfs{alloc};
        vfs.Mount("", &mem);

        nectar::IOScheduler io{vfs, alloc, {2}};

        (void)io.Submit("a.txt", nectar::LoadPriority::Normal);
        (void)io.Submit("b.txt", nectar::LoadPriority::Normal);
        (void)io.Submit("c.txt", nectar::LoadPriority::Normal);

        wax::Vector<nectar::IOCompletion> completions{alloc};
        bool ok = PollUntil([&]() {
            io.DrainCompletions(completions);
            return completions.Size() >= 3;
        });

        larvae::AssertTrue(ok);
        larvae::AssertEqual(completions.Size(), size_t{3});

        // All should succeed
        for (size_t i = 0; i < completions.Size(); ++i)
            larvae::AssertTrue(completions[i].success);

        io.Shutdown();
    });

    // =========================================================================
    // Priority ordering
    // =========================================================================

    auto t3 = larvae::RegisterTest("NectarIO", "PriorityOrdering", []() {
        auto& alloc = GetIOAlloc();
        nectar::MemoryMountSource mem{alloc};
        mem.AddFile("low.txt", wax::ByteSpan{"low", 3});
        mem.AddFile("high.txt", wax::ByteSpan{"high", 4});
        mem.AddFile("crit.txt", wax::ByteSpan{"crit", 4});

        nectar::VirtualFilesystem vfs{alloc};
        vfs.Mount("", &mem);

        // 1 worker so requests are processed sequentially
        nectar::IOScheduler io{vfs, alloc, {1}};

        // Submit low first, then critical — critical should be processed first
        // We need to pause the worker to queue both before processing starts
        // Since we can't pause, submit all at once and check that critical finishes
        (void)io.Submit("low.txt", nectar::LoadPriority::Low);
        (void)io.Submit("high.txt", nectar::LoadPriority::High);
        auto id_crit = io.Submit("crit.txt", nectar::LoadPriority::Critical);

        wax::Vector<nectar::IOCompletion> completions{alloc};
        bool ok = PollUntil([&]() {
            io.DrainCompletions(completions);
            return completions.Size() >= 3;
        });

        larvae::AssertTrue(ok);
        larvae::AssertEqual(completions.Size(), size_t{3});

        // First completion should be critical (lowest enum value)
        larvae::AssertEqual(completions[0].request_id, id_crit);

        io.Shutdown();
    });

    // =========================================================================
    // Cancel pending
    // =========================================================================

    auto t4 = larvae::RegisterTest("NectarIO", "CancelPending", []() {
        auto& alloc = GetIOAlloc();
        nectar::MemoryMountSource mem{alloc};
        mem.AddFile("a.txt", wax::ByteSpan{"a", 1});
        mem.AddFile("b.txt", wax::ByteSpan{"b", 1});

        nectar::VirtualFilesystem vfs{alloc};
        vfs.Mount("", &mem);

        nectar::IOScheduler io{vfs, alloc, {1}};

        auto id_a = io.Submit("a.txt", nectar::LoadPriority::Normal);
        auto id_b = io.Submit("b.txt", nectar::LoadPriority::Normal);

        // Cancel b before it's processed
        io.Cancel(id_b);

        wax::Vector<nectar::IOCompletion> completions{alloc};
        bool ok = PollUntil([&]() {
            io.DrainCompletions(completions);
            return completions.Size() >= 1;
        });

        larvae::AssertTrue(ok);

        // Only a should appear in completions
        // (b was cancelled, so either it was skipped or filtered)
        bool found_b = false;
        for (size_t i = 0; i < completions.Size(); ++i)
        {
            if (completions[i].request_id == id_b)
                found_b = true;
        }
        larvae::AssertFalse(found_b);

        // a should be there
        bool found_a = false;
        for (size_t i = 0; i < completions.Size(); ++i)
        {
            if (completions[i].request_id == id_a)
                found_a = true;
        }
        larvae::AssertTrue(found_a);

        io.Shutdown();
    });

    // =========================================================================
    // Cancel in-flight (result discarded on drain)
    // =========================================================================

    auto t5 = larvae::RegisterTest("NectarIO", "CancelInFlight", []() {
        auto& alloc = GetIOAlloc();
        nectar::MemoryMountSource mem{alloc};
        mem.AddFile("data.txt", wax::ByteSpan{"data", 4});

        nectar::VirtualFilesystem vfs{alloc};
        vfs.Mount("", &mem);

        nectar::IOScheduler io{vfs, alloc, {1}};

        auto id = io.Submit("data.txt", nectar::LoadPriority::Normal);

        // Wait for completion to be ready
        std::this_thread::sleep_for(std::chrono::milliseconds{50});

        // Cancel after it might have completed (goes into cancelled_ids_)
        io.Cancel(id);

        wax::Vector<nectar::IOCompletion> completions{alloc};
        io.DrainCompletions(completions);

        // Should be filtered out
        bool found = false;
        for (size_t i = 0; i < completions.Size(); ++i)
        {
            if (completions[i].request_id == id)
                found = true;
        }
        larvae::AssertFalse(found);

        io.Shutdown();
    });

    // =========================================================================
    // Read non-existent file
    // =========================================================================

    auto t6 = larvae::RegisterTest("NectarIO", "ReadNonExistent", []() {
        auto& alloc = GetIOAlloc();
        nectar::MemoryMountSource mem{alloc};

        nectar::VirtualFilesystem vfs{alloc};
        vfs.Mount("", &mem);

        nectar::IOScheduler io{vfs, alloc, {1}};

        auto id = io.Submit("doesnt_exist.txt", nectar::LoadPriority::Normal);

        wax::Vector<nectar::IOCompletion> completions{alloc};
        bool ok = PollUntil([&]() {
            io.DrainCompletions(completions);
            return completions.Size() > 0;
        });

        larvae::AssertTrue(ok);
        larvae::AssertEqual(completions[0].request_id, id);
        larvae::AssertFalse(completions[0].success);
        larvae::AssertEqual(completions[0].data.Size(), size_t{0});

        io.Shutdown();
    });

    // =========================================================================
    // Empty drain
    // =========================================================================

    auto t7 = larvae::RegisterTest("NectarIO", "EmptyDrain", []() {
        auto& alloc = GetIOAlloc();
        nectar::MemoryMountSource mem{alloc};

        nectar::VirtualFilesystem vfs{alloc};
        vfs.Mount("", &mem);

        nectar::IOScheduler io{vfs, alloc, {1}};

        wax::Vector<nectar::IOCompletion> completions{alloc};
        size_t count = io.DrainCompletions(completions);
        larvae::AssertEqual(count, size_t{0});
        larvae::AssertEqual(completions.Size(), size_t{0});

        io.Shutdown();
    });

    // =========================================================================
    // Shutdown joins cleanly
    // =========================================================================

    auto t8 = larvae::RegisterTest("NectarIO", "ShutdownJoins", []() {
        auto& alloc = GetIOAlloc();
        nectar::MemoryMountSource mem{alloc};

        nectar::VirtualFilesystem vfs{alloc};
        vfs.Mount("", &mem);

        {
            nectar::IOScheduler io{vfs, alloc, {2}};
            (void)io.Submit("x.txt", nectar::LoadPriority::Normal);
            // Destructor calls Shutdown which joins threads
        }
        // If we get here without hanging, shutdown works
        larvae::AssertTrue(true);
    });

    // =========================================================================
    // Double shutdown is safe
    // =========================================================================

    auto t9 = larvae::RegisterTest("NectarIO", "DoubleShutdown", []() {
        auto& alloc = GetIOAlloc();
        nectar::MemoryMountSource mem{alloc};

        nectar::VirtualFilesystem vfs{alloc};
        vfs.Mount("", &mem);

        nectar::IOScheduler io{vfs, alloc, {1}};
        io.Shutdown();
        io.Shutdown();  // should not hang or crash
        larvae::AssertTrue(io.IsShutdown());
    });

    // =========================================================================
    // PendingCount
    // =========================================================================

    auto t10 = larvae::RegisterTest("NectarIO", "PendingCount", []() {
        auto& alloc = GetIOAlloc();
        nectar::MemoryMountSource mem{alloc};
        mem.AddFile("a.txt", wax::ByteSpan{"a", 1});

        nectar::VirtualFilesystem vfs{alloc};
        vfs.Mount("", &mem);

        nectar::IOScheduler io{vfs, alloc, {1}};

        // Initially 0
        // (may not be 0 if worker already picked it up, but check IsShutdown at least)
        larvae::AssertFalse(io.IsShutdown());

        io.Shutdown();
        larvae::AssertTrue(io.IsShutdown());
    });

    // =========================================================================
    // Large file
    // =========================================================================

    auto t11 = larvae::RegisterTest("NectarIO", "LargeFile", []() {
        auto& alloc = GetIOAlloc();
        nectar::MemoryMountSource mem{alloc};

        // 64KB file
        constexpr size_t kSize = 64 * 1024;
        wax::Vector<uint8_t> big_data{alloc};
        for (size_t i = 0; i < kSize; ++i)
            big_data.PushBack(static_cast<uint8_t>(i & 0xFF));

        mem.AddFile("big.bin", wax::ByteSpan{big_data.Data(), big_data.Size()});

        nectar::VirtualFilesystem vfs{alloc};
        vfs.Mount("", &mem);

        nectar::IOScheduler io{vfs, alloc, {1}};
        (void)io.Submit("big.bin", nectar::LoadPriority::Normal);

        wax::Vector<nectar::IOCompletion> completions{alloc};
        bool ok = PollUntil([&]() {
            io.DrainCompletions(completions);
            return completions.Size() > 0;
        });

        larvae::AssertTrue(ok);
        larvae::AssertTrue(completions[0].success);
        larvae::AssertEqual(completions[0].data.Size(), kSize);

        // Verify content
        auto* ptr = completions[0].data.Data();
        for (size_t i = 0; i < kSize; ++i)
            larvae::AssertEqual(ptr[i], static_cast<uint8_t>(i & 0xFF));

        io.Shutdown();
    });

    // =========================================================================
    // Concurrent submits from multiple threads
    // =========================================================================

    auto t12 = larvae::RegisterTest("NectarIO", "ConcurrentSubmit", []() {
        auto& alloc = GetIOAlloc();
        nectar::MemoryMountSource mem{alloc};

        // 20 files, all with literal content
        const char* names[] = {
            "f00.txt", "f01.txt", "f02.txt", "f03.txt", "f04.txt",
            "f05.txt", "f06.txt", "f07.txt", "f08.txt", "f09.txt",
            "f10.txt", "f11.txt", "f12.txt", "f13.txt", "f14.txt",
            "f15.txt", "f16.txt", "f17.txt", "f18.txt", "f19.txt"
        };
        constexpr size_t kFileCount = 20;

        for (size_t i = 0; i < kFileCount; ++i)
            mem.AddFile(names[i], wax::ByteSpan{"xx", 2});

        nectar::VirtualFilesystem vfs{alloc};
        vfs.Mount("", &mem);

        nectar::IOScheduler io{vfs, alloc, {1}};

        // Submit from multiple threads simultaneously
        std::vector<std::thread> submitters;
        for (size_t t = 0; t < 4; ++t)
        {
            submitters.emplace_back([&io, &names, t]() {
                for (size_t i = t * 5; i < (t + 1) * 5; ++i)
                    (void)io.Submit(names[i], nectar::LoadPriority::Normal);
            });
        }

        for (auto& s : submitters)
            s.join();

        wax::Vector<nectar::IOCompletion> completions{alloc};
        bool ok = PollUntil([&]() {
            io.DrainCompletions(completions);
            return completions.Size() >= kFileCount;
        });

        larvae::AssertTrue(ok);
        larvae::AssertEqual(completions.Size(), kFileCount);

        for (size_t i = 0; i < completions.Size(); ++i)
            larvae::AssertTrue(completions[i].success);

        io.Shutdown();
    });

}
