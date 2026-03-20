#pragma once

#include <hive/core/assert.h>

#include <atomic>

namespace hive
{
    template <typename T> class Singleton
    {
    public:
        Singleton()
        {
            T* expected = nullptr;
            g_mInstance.compare_exchange_strong(expected, static_cast<T*>(this), std::memory_order_release,
                                               std::memory_order_relaxed);
        }

        ~Singleton()
        {
            T* expected = static_cast<T*>(this);
            g_mInstance.compare_exchange_strong(expected, nullptr, std::memory_order_release,
                                               std::memory_order_relaxed);
        }

        static T& GetInstance()
        {
            T* ptr = g_mInstance.load(std::memory_order_acquire);
            hive::Assert(ptr != nullptr, "Singleton::GetInstance() called before initialization");
            return *ptr;
        }

        [[nodiscard]] static bool IsInitialized()
        {
            return g_mInstance.load(std::memory_order_acquire) != nullptr;
        }

    protected:
        static inline std::atomic<T*> g_mInstance{nullptr};
    };
} // namespace hive
