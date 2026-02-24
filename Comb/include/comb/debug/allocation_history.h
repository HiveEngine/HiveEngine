/**
 * Allocation History (Ring Buffer)
 *
 * Keeps a ring buffer of recent allocations for post-mortem debugging.
 * Useful for analyzing allocation patterns after crashes or assertions.
 *
 * Design:
 * - Ring buffer: Fixed-size circular buffer (default 1000 entries)
 * - Thread-safe: Protected by mutex
 * - Configurable: Size controlled by COMB_MEM_DEBUG_HISTORY_SIZE
 * - Zero overhead: Only compiled when COMB_MEM_DEBUG_HISTORY=1
 *
 * Features:
 * - RecordAllocation: Add allocation to history
 * - RecordDeallocation: Add deallocation to history
 * - DumpToLog: Print recent history to log
 * - DumpToFile: Save history to file (for crash dumps)
 *
 * Example:
 * @code
 *   #if COMB_MEM_DEBUG_HISTORY
 *       comb::debug::AllocationHistory history;
 *
 *       // Record allocation
 *       history.RecordAllocation(info);
 *
 *       // Record deallocation
 *       history.RecordDeallocation(ptr, size);
 *
 *       // Dump on crash
 *       if (crashed) {
 *           history.DumpToFile("crash_memory_history.txt");
 *       }
 *   #endif
 * @endcode
 */

#pragma once

#include <comb/debug/mem_debug_config.h>

#if COMB_MEM_DEBUG && COMB_MEM_DEBUG_HISTORY

#include <comb/debug/allocation_info.h>
#include <comb/debug/platform_utils.h>
#include <hive/core/log.h>
#include <comb/combmodule.h>

#include <array>
#include <mutex>
#include <fstream>

namespace comb::debug
{

enum class HistoryEventType : uint8_t
{
    Allocation,
    Deallocation
};

/**
 * History entry (one allocation or deallocation event)
 */
struct HistoryEntry
{
    HistoryEventType type{HistoryEventType::Allocation};
    void* address{nullptr};
    size_t size{0};
    uint64_t timestamp{0};
    const char* tag{nullptr};
    uint32_t threadId{0};
    uint32_t allocationId{0};
};

/**
 * Allocation history ring buffer
 *
 * Thread-safe circular buffer that records recent allocation/deallocation events.
 * Useful for post-mortem analysis after crashes.
 *
 * Size: COMB_MEM_DEBUG_HISTORY_SIZE entries (default 1000)
 * Memory overhead: ~48 bytes per entry (total ~48 KB for 1000 entries)
 */
class AllocationHistory
{
public:
    AllocationHistory() = default;
    ~AllocationHistory() = default;

    // Non-copyable, non-movable (contains std::mutex)
    AllocationHistory(const AllocationHistory&) = delete;
    AllocationHistory& operator=(const AllocationHistory&) = delete;
    AllocationHistory(AllocationHistory&&) noexcept = delete;
    AllocationHistory& operator=(AllocationHistory&&) noexcept = delete;

    // ========================================================================
    // Recording API
    // ========================================================================

    /**
     * Record an allocation event
     *
     * Adds entry to ring buffer (overwrites oldest entry when full).
     *
     * @param info Allocation metadata
     *
     * Thread-safe: Yes (mutex protected)
     */
    void RecordAllocation(const AllocationInfo& info)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        HistoryEntry entry{};
        entry.type = HistoryEventType::Allocation;
        entry.address = info.address;
        entry.size = info.size;
        entry.timestamp = info.timestamp;
        entry.tag = info.tag;
        entry.threadId = info.threadId;
        entry.allocationId = info.allocationId;

        AddEntry(entry);
    }

    /**
     * Record a deallocation event
     *
     * @param address User pointer
     * @param size Size of allocation
     *
     * Thread-safe: Yes (mutex protected)
     */
    void RecordDeallocation(void* address, size_t size)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        HistoryEntry entry{};
        entry.type = HistoryEventType::Deallocation;
        entry.address = address;
        entry.size = size;
        entry.timestamp = GetTimestamp();
        entry.threadId = GetThreadId();

        AddEntry(entry);
    }

    // ========================================================================
    // Query API
    // ========================================================================

    /**
     * Get number of entries in history (0 to COMB_MEM_DEBUG_HISTORY_SIZE)
     */
    [[nodiscard]] size_t GetEntryCount() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_;
    }

    /**
     * Check if history is empty
     */
    [[nodiscard]] bool IsEmpty() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_ == 0;
    }

    /**
     * Check if history is full (reached capacity)
     */
    [[nodiscard]] bool IsFull() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return count_ >= COMB_MEM_DEBUG_HISTORY_SIZE;
    }

    /**
     * Clear all history
     */
    void Clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        count_ = 0;
        writeIndex_ = 0;
    }

    // ========================================================================
    // Dump API
    // ========================================================================

    /**
     * Dump history to log
     *
     * Prints recent allocation/deallocation events to hive::Log.
     *
     * @param allocatorName Name of allocator (for logging)
     * @param maxEntries Max entries to print (0 = all)
     */
    void DumpToLog(const char* allocatorName, size_t maxEntries = 100) const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (count_ == 0)
        {
            hive::LogInfo(comb::LogCombRoot,
                          "[MEM_DEBUG] [{}] Allocation history is empty",
                          allocatorName);
            return;
        }

        size_t entriesToPrint = (maxEntries == 0 || maxEntries > count_) ? count_ : maxEntries;

        hive::LogInfo(comb::LogCombRoot,
                      "[MEM_DEBUG] [{}] Recent allocation history ({} / {} entries):",
                      allocatorName, entriesToPrint, count_);

        // Print in chronological order (oldest first)
        size_t startIndex = count_ < COMB_MEM_DEBUG_HISTORY_SIZE ? 0 : writeIndex_;
        size_t printed = 0;

        for (size_t i = 0; i < count_ && printed < entriesToPrint; ++i)
        {
            size_t index = (startIndex + i) % COMB_MEM_DEBUG_HISTORY_SIZE;
            const HistoryEntry& entry = history_[index];

            const char* eventType = (entry.type == HistoryEventType::Allocation) ? "ALLOC" : "FREE ";

            hive::LogInfo(comb::LogCombRoot,
                          "  [{}] #{}: Address={}, Size={} bytes, Tag={}, Thread={}",
                          eventType, entry.allocationId, entry.address, entry.size,
                          entry.tag ? entry.tag : "<no tag>", entry.threadId);

            printed++;
        }
    }

    /**
     * Dump history to file
     *
     * Saves history to text file for post-mortem analysis.
     *
     * @param filename Output file path
     * @return true if successful, false otherwise
     */
    bool DumpToFile(const char* filename) const
    {
        std::lock_guard<std::mutex> lock(mutex_);

        std::ofstream file(filename);
        if (!file.is_open())
        {
            hive::LogError(comb::LogCombRoot,
                           "[MEM_DEBUG] Failed to open file for history dump: {}", filename);
            return false;
        }

        file << "=== Comb Allocation History Dump ===\n";
        file << "Entries: " << count_ << " / " << COMB_MEM_DEBUG_HISTORY_SIZE << "\n";
        file << "Timestamp: " << GetTimestamp() << " ns\n";
        file << "=====================================\n\n";

        if (count_ == 0)
        {
            file << "(no entries)\n";
            file.close();
            return true;
        }

        // Dump in chronological order
        size_t startIndex = count_ < COMB_MEM_DEBUG_HISTORY_SIZE ? 0 : writeIndex_;

        for (size_t i = 0; i < count_; ++i)
        {
            size_t index = (startIndex + i) % COMB_MEM_DEBUG_HISTORY_SIZE;
            const HistoryEntry& entry = history_[index];

            const char* eventType = (entry.type == HistoryEventType::Allocation) ? "ALLOC" : "FREE ";

            file << "[" << eventType << "] "
                 << "#" << entry.allocationId << ": "
                 << "Address=" << entry.address << ", "
                 << "Size=" << entry.size << " bytes, "
                 << "Tag=" << (entry.tag ? entry.tag : "<no tag>") << ", "
                 << "Thread=" << entry.threadId << ", "
                 << "Timestamp=" << entry.timestamp << " ns\n";
        }

        file.close();

        hive::LogInfo(comb::LogCombRoot,
                      "[MEM_DEBUG] History dumped to file: {}", filename);

        return true;
    }

private:
    // Assumes mutex held
    void AddEntry(const HistoryEntry& entry)
    {
        history_[writeIndex_] = entry;
        writeIndex_ = (writeIndex_ + 1) % COMB_MEM_DEBUG_HISTORY_SIZE;

        if (count_ < COMB_MEM_DEBUG_HISTORY_SIZE)
        {
            count_++;
        }
    }

    std::array<HistoryEntry, COMB_MEM_DEBUG_HISTORY_SIZE> history_{};
    size_t count_{0};
    size_t writeIndex_{0};
    mutable std::mutex mutex_;
};

} // namespace comb::debug

#endif // COMB_MEM_DEBUG && COMB_MEM_DEBUG_HISTORY
