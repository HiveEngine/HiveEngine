#pragma once

#include <hive/hive_config.h>
#include <hive/core/assert.h>

#include <atomic>
#include <cstdint>
#include <string_view>

namespace hive
{
    namespace detail
    {
        constexpr uint64_t FnvHash(std::string_view str) noexcept
        {
            uint64_t hash = 14695981039346656037ULL;
            for (char c : str)
            {
                hash ^= static_cast<uint64_t>(c);
                hash *= 1099511628211ULL;
            }
            return hash;
        }

        template <typename T> constexpr uint64_t SingletonTypeId()
        {
#if defined(__clang__) || defined(__GNUC__)
            return FnvHash(__PRETTY_FUNCTION__);
#elif defined(_MSC_VER)
            return FnvHash(__FUNCSIG__);
#else
            static_assert(false, "Unsupported compiler for SingletonTypeId");
#endif
        }

        HIVE_API std::atomic<void*>& GetSingletonSlot(uint64_t typeId);
    } // namespace detail

    // Base class for engine singletons. Storage is centralized in a single
    // exported registry so gameplay DLLs share the same instances as the host.
    template <typename T> class Singleton
    {
    public:
        Singleton()
        {
            void* expected = nullptr;
            Slot().compare_exchange_strong(expected, static_cast<void*>(static_cast<T*>(this)),
                                           std::memory_order_release, std::memory_order_relaxed);
        }

        ~Singleton()
        {
            void* expected = static_cast<void*>(static_cast<T*>(this));
            Slot().compare_exchange_strong(expected, static_cast<void*>(nullptr), std::memory_order_release,
                                           std::memory_order_relaxed);
        }

        static T& GetInstance()
        {
            void* ptr = Slot().load(std::memory_order_acquire);
            hive::Assert(ptr != nullptr, "Singleton::GetInstance() called before initialization");
            return *static_cast<T*>(ptr);
        }

        [[nodiscard]] static bool IsInitialized()
        {
            return Slot().load(std::memory_order_acquire) != nullptr;
        }

    private:
        static std::atomic<void*>& Slot()
        {
            return detail::GetSingletonSlot(detail::SingletonTypeId<T>());
        }
    };
} // namespace hive
