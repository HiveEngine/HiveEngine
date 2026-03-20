#pragma once

#include <hive/core/assert.h>

#include <comb/buddy_allocator.h>

#include <cstddef>

namespace comb
{
    class ChainedBuddyAllocator
    {
    public:
        static constexpr size_t kMaxBlocks = 32;

        using OomCallback = void (*)(size_t requestedSize);

        ChainedBuddyAllocator(size_t blockSize, size_t hardCap, const char* debugName = "ChainedBuddyAllocator")
            : m_blockSize{blockSize}
            , m_hardCap{hardCap}
            , m_debugName{debugName}
        {
            AddBlock();
        }

        ~ChainedBuddyAllocator()
        {
            for (size_t i = 0; i < m_blockCount; ++i)
            {
                delete m_blocks[i];
            }
        }

        ChainedBuddyAllocator(const ChainedBuddyAllocator&) = delete;
        ChainedBuddyAllocator& operator=(const ChainedBuddyAllocator&) = delete;

        ChainedBuddyAllocator(ChainedBuddyAllocator&& other) noexcept
            : m_blockSize{other.m_blockSize}
            , m_hardCap{other.m_hardCap}
            , m_blockCount{other.m_blockCount}
            , m_debugName{other.m_debugName}
            , m_oomCallback{other.m_oomCallback}
        {
            for (size_t i = 0; i < kMaxBlocks; ++i)
            {
                m_blocks[i] = other.m_blocks[i];
                other.m_blocks[i] = nullptr;
            }
            other.m_blockCount = 0;
        }

        ChainedBuddyAllocator& operator=(ChainedBuddyAllocator&& other) noexcept
        {
            if (this != &other)
            {
                for (size_t i = 0; i < m_blockCount; ++i)
                    delete m_blocks[i];

                m_blockSize = other.m_blockSize;
                m_hardCap = other.m_hardCap;
                m_blockCount = other.m_blockCount;
                m_debugName = other.m_debugName;
                m_oomCallback = other.m_oomCallback;

                for (size_t i = 0; i < kMaxBlocks; ++i)
                {
                    m_blocks[i] = other.m_blocks[i];
                    other.m_blocks[i] = nullptr;
                }
                other.m_blockCount = 0;
            }
            return *this;
        }

        [[nodiscard]] void* Allocate(size_t size, size_t alignment, const char* tag = nullptr)
        {
            for (size_t i = 0; i < m_blockCount; ++i)
            {
                void* ptr = m_blocks[i]->Allocate(size, alignment, tag);
                if (ptr)
                    return ptr;
            }

            if (!AddBlock())
            {
                if (m_oomCallback)
                    m_oomCallback(size);
                return nullptr;
            }

            return m_blocks[m_blockCount - 1]->Allocate(size, alignment, tag);
        }

        void Deallocate(void* ptr)
        {
            if (!ptr)
                return;

            auto* bytePtr = static_cast<std::byte*>(ptr);
            for (size_t i = 0; i < m_blockCount; ++i)
            {
                auto* base = static_cast<std::byte*>(m_blocks[i]->GetBaseAddress());
                size_t capacity = m_blocks[i]->GetTotalMemory();
                if (bytePtr >= base && bytePtr < base + capacity)
                {
                    m_blocks[i]->Deallocate(ptr);
                    return;
                }
            }

            hive::Assert(false, "ChainedBuddyAllocator: pointer not owned by any block");
        }

        [[nodiscard]] size_t GetUsedMemory() const noexcept
        {
            size_t total = 0;
            for (size_t i = 0; i < m_blockCount; ++i)
                total += m_blocks[i]->GetUsedMemory();
            return total;
        }

        [[nodiscard]] size_t GetTotalMemory() const noexcept
        {
            size_t total = 0;
            for (size_t i = 0; i < m_blockCount; ++i)
                total += m_blocks[i]->GetTotalMemory();
            return total;
        }

        [[nodiscard]] const char* GetName() const noexcept
        {
            return m_debugName;
        }

        void SetOomCallback(OomCallback cb) noexcept
        {
            m_oomCallback = cb;
        }

    private:
        bool AddBlock()
        {
            if (m_blockCount >= kMaxBlocks)
                return false;

            size_t currentTotal = 0;
            for (size_t i = 0; i < m_blockCount; ++i)
                currentTotal += m_blocks[i]->GetTotalMemory();

            if (currentTotal + m_blockSize > m_hardCap)
                return false;

            m_blocks[m_blockCount] = new BuddyAllocator{m_blockSize, m_debugName};
            ++m_blockCount;
            return true;
        }

        size_t m_blockSize;
        size_t m_hardCap;
        size_t m_blockCount{0};
        const char* m_debugName;
        OomCallback m_oomCallback{nullptr};
        BuddyAllocator* m_blocks[kMaxBlocks]{};
    };

    static_assert(Allocator<ChainedBuddyAllocator>, "ChainedBuddyAllocator must satisfy Allocator concept");
} // namespace comb
