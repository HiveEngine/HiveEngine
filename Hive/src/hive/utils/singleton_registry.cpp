#include <hive/utils/singleton.h>

#include <atomic>
#include <cstdint>

namespace hive::detail
{
    static constexpr size_t kMaxSlots = 64;
    static std::atomic<void*> s_slots[kMaxSlots]{};
    static std::atomic<uint64_t> s_keys[kMaxSlots]{};

    std::atomic<void*>& GetSingletonSlot(uint64_t typeId)
    {
        for (size_t i = 0; i < kMaxSlots; ++i)
        {
            uint64_t expected = 0;
            uint64_t current = s_keys[i].load(std::memory_order_acquire);

            if (current == typeId)
                return s_slots[i];

            if (current == 0 && s_keys[i].compare_exchange_strong(expected, typeId, std::memory_order_acq_rel))
                return s_slots[i];
        }

        hive::Assert(false, "Singleton registry full");
        return s_slots[0];
    }
} // namespace hive::detail
