#pragma once

#include <hive/core/assert.h>

#include <comb/default_allocator.h>
#include <comb/new.h>

#include <atomic>
#include <type_traits>
#include <utility>

namespace wax
{
    /**
     * Shared ownership smart pointer with atomic reference counting
     *
     * Arc<T, Allocator> is the thread-safe counterpart to Rc<T>.
     * Uses std::atomic<size_t> for the reference count so copies
     * and destruction are safe from any thread.
     *
     * The pointed-to object itself is NOT synchronized —
     * callers must coordinate access if multiple threads mutate it.
     *
     * Memory ordering:
     * - Copy (increment): relaxed — no data needs to be visible yet
     * - Destroy (decrement): acq_rel — must see all prior writes before destroying
     */
    template <typename T, comb::Allocator Allocator = comb::DefaultAllocator> class Arc
    {
    private:
        struct ControlBlock
        {
            std::atomic<size_t> m_refCount;
            T m_object;

            template <typename... Args>
            ControlBlock(Args&&... args)
                : m_refCount{1}
                , m_object{std::forward<Args>(args)...}
            {
            }
        };

        template <typename U, comb::Allocator A, typename... Args>
        friend Arc<U, A> MakeArc(A& allocator, Args&&... args);

        template <typename U, typename... Args> friend Arc<U, comb::DefaultAllocator> MakeArc(Args&&... args);

    public:
        using ValueType = T;
        using AllocatorType = Allocator;

        constexpr Arc() noexcept
            : m_control{nullptr}
            , m_allocator{nullptr}
        {
        }

        constexpr Arc(Allocator& allocator, ControlBlock* control) noexcept
            : m_control{control}
            , m_allocator{&allocator}
        {
        }

        Arc(const Arc& other) noexcept
            : m_control{other.m_control}
            , m_allocator{other.m_allocator}
        {
            if (m_control)
            {
                m_control->m_refCount.fetch_add(1, std::memory_order_relaxed);
            }
        }

        Arc& operator=(const Arc& other) noexcept
        {
            if (this != &other)
            {
                Release();
                m_control = other.m_control;
                m_allocator = other.m_allocator;
                if (m_control)
                {
                    m_control->m_refCount.fetch_add(1, std::memory_order_relaxed);
                }
            }
            return *this;
        }

        constexpr Arc(Arc&& other) noexcept
            : m_control{other.m_control}
            , m_allocator{other.m_allocator}
        {
            other.m_control = nullptr;
            other.m_allocator = nullptr;
        }

        constexpr Arc& operator=(Arc&& other) noexcept
        {
            if (this != &other)
            {
                Release();
                m_control = other.m_control;
                m_allocator = other.m_allocator;
                other.m_control = nullptr;
                other.m_allocator = nullptr;
            }
            return *this;
        }

        ~Arc() noexcept
        {
            Release();
        }

        [[nodiscard]] T& operator*() const noexcept
        {
            hive::Assert(m_control != nullptr, "Dereferencing null Arc");
            return m_control->m_object;
        }

        [[nodiscard]] T* operator->() const noexcept
        {
            hive::Assert(m_control != nullptr, "Dereferencing null Arc");
            return &m_control->m_object;
        }

        [[nodiscard]] T* Get() const noexcept
        {
            return m_control ? &m_control->m_object : nullptr;
        }

        [[nodiscard]] Allocator* GetAllocator() const noexcept
        {
            return m_allocator;
        }

        [[nodiscard]] size_t GetRefCount() const noexcept
        {
            return m_control ? m_control->m_refCount.load(std::memory_order_relaxed) : 0;
        }

        [[nodiscard]] bool IsUnique() const noexcept
        {
            return m_control && m_control->m_refCount.load(std::memory_order_relaxed) == 1;
        }

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return m_control != nullptr;
        }

        [[nodiscard]] bool IsNull() const noexcept
        {
            return m_control == nullptr;
        }

        [[nodiscard]] bool IsValid() const noexcept
        {
            return m_control != nullptr;
        }

        void Reset() noexcept
        {
            Release();
            m_control = nullptr;
            m_allocator = nullptr;
        }

        [[nodiscard]] bool operator==(const Arc& other) const noexcept
        {
            return m_control == other.m_control;
        }

        [[nodiscard]] bool operator==(std::nullptr_t) const noexcept
        {
            return m_control == nullptr;
        }

    private:
        void Release() noexcept
        {
            if (m_control && m_allocator)
            {
                if (m_control->m_refCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
                {
                    m_control->m_object.~T();
                    m_allocator->Deallocate(m_control);
                }
            }
        }

        ControlBlock* m_control;
        Allocator* m_allocator;
    };

    template <typename T, comb::Allocator Allocator, typename... Args>
    [[nodiscard]] Arc<T, Allocator> MakeArc(Allocator& allocator, Args&&... args)
    {
        using ControlBlock = typename Arc<T, Allocator>::ControlBlock;

        void* mem = allocator.Allocate(sizeof(ControlBlock), alignof(ControlBlock));
        hive::Check(mem != nullptr, "Arc allocation failed");

        ControlBlock* control = new (mem) ControlBlock(std::forward<Args>(args)...);

        return Arc<T, Allocator>{allocator, control};
    }

    template <typename T, typename... Args> [[nodiscard]] Arc<T, comb::DefaultAllocator> MakeArc(Args&&... args)
    {
        auto& allocator = comb::GetDefaultAllocator();
        using ControlBlock = typename Arc<T, comb::DefaultAllocator>::ControlBlock;

        void* mem = allocator.Allocate(sizeof(ControlBlock), alignof(ControlBlock));
        hive::Check(mem != nullptr, "Arc allocation failed");

        ControlBlock* control = new (mem) ControlBlock(std::forward<Args>(args)...);

        return Arc<T, comb::DefaultAllocator>{allocator, control};
    }
} // namespace wax
