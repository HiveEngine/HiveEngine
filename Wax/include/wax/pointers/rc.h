#pragma once

#include <hive/core/assert.h>

#include <comb/default_allocator.h>
#include <comb/new.h>

#include <type_traits>
#include <utility>

namespace wax
{
    // Shared-ownership smart pointer with non-atomic reference counting.
    // NOT thread-safe — use Arc<T> for cross-thread sharing.
    template <typename T, comb::Allocator Allocator = comb::DefaultAllocator> class Rc
    {
    private:
        struct ControlBlock
        {
            size_t ref_count;
            T object;

            template <typename... Args>
            ControlBlock(Args&&... args)
                : ref_count{1}
                , object{std::forward<Args>(args)...}
            {
            }
        };

        template <typename U, comb::Allocator A, typename... Args> friend Rc<U, A> MakeRc(A& allocator, Args&&... args);

        template <typename U, typename... Args> friend Rc<U, comb::DefaultAllocator> MakeRc(Args&&... args);

    public:
        using ValueType = T;
        using AllocatorType = Allocator;

        constexpr Rc() noexcept
            : m_control{nullptr}
            , m_allocator{nullptr}
        {
        }

        constexpr Rc(Allocator& allocator, ControlBlock* control) noexcept
            : m_control{control}
            , m_allocator{&allocator}
        {
        }

        Rc(const Rc& other) noexcept
            : m_control{other.m_control}
            , m_allocator{other.m_allocator}
        {
            if (m_control)
            {
                ++m_control->ref_count;
            }
        }

        Rc& operator=(const Rc& other) noexcept
        {
            if (this != &other)
            {
                Release();

                m_control = other.m_control;
                m_allocator = other.m_allocator;

                if (m_control)
                {
                    ++m_control->ref_count;
                }
            }
            return *this;
        }

        constexpr Rc(Rc&& other) noexcept
            : m_control{other.m_control}
            , m_allocator{other.m_allocator}
        {
            other.m_control = nullptr;
            other.m_allocator = nullptr;
        }

        constexpr Rc& operator=(Rc&& other) noexcept
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

        ~Rc() noexcept
        {
            Release();
        }

        [[nodiscard]] T& operator*() const noexcept
        {
            hive::Assert(m_control != nullptr, "Dereferencing null Rc");
            return m_control->object;
        }

        [[nodiscard]] T* operator->() const noexcept
        {
            hive::Assert(m_control != nullptr, "Dereferencing null Rc");
            return &m_control->object;
        }

        [[nodiscard]] T* Get() const noexcept
        {
            return m_control ? &m_control->object : nullptr;
        }

        [[nodiscard]] Allocator* GetAllocator() const noexcept
        {
            return m_allocator;
        }

        [[nodiscard]] size_t GetRefCount() const noexcept
        {
            return m_control ? m_control->ref_count : 0;
        }

        [[nodiscard]] bool IsUnique() const noexcept
        {
            return m_control && m_control->ref_count == 1;
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

        [[nodiscard]] bool operator==(const Rc& other) const noexcept
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
                --m_control->ref_count;

                if (m_control->ref_count == 0)
                {
                    m_control->object.~T();
                    m_allocator->Deallocate(m_control);
                }
            }
        }

        ControlBlock* m_control;
        Allocator* m_allocator;
    };

    template <typename T, comb::Allocator Allocator, typename... Args>
    [[nodiscard]] Rc<T, Allocator> MakeRc(Allocator& allocator, Args&&... args)
    {
        using ControlBlock = typename Rc<T, Allocator>::ControlBlock;

        void* mem = allocator.Allocate(sizeof(ControlBlock), alignof(ControlBlock));
        hive::Check(mem != nullptr, "Rc allocation failed");

        ControlBlock* control = new (mem) ControlBlock(std::forward<Args>(args)...);

        return Rc<T, Allocator>{allocator, control};
    }

    template <typename T, typename... Args> [[nodiscard]] Rc<T, comb::DefaultAllocator> MakeRc(Args&&... args)
    {
        auto& allocator = comb::GetDefaultAllocator();
        using ControlBlock = typename Rc<T, comb::DefaultAllocator>::ControlBlock;

        void* mem = allocator.Allocate(sizeof(ControlBlock), alignof(ControlBlock));
        hive::Check(mem != nullptr, "Rc allocation failed");

        ControlBlock* control = new (mem) ControlBlock(std::forward<Args>(args)...);

        return Rc<T, comb::DefaultAllocator>{allocator, control};
    }
} // namespace wax
