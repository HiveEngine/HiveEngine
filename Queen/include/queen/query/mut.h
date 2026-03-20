#pragma once

#include <queen/core/tick.h>

#include <type_traits>

namespace queen
{
    /**
     * Mutable component reference with automatic change tracking
     *
     * Mut<T> wraps a mutable reference to a component and automatically marks
     * it as changed when accessed through operator-> or operator*.
     * This enables efficient change detection: only components that were
     * actually accessed are marked as changed.
     *
     * The change marking happens when:
     * - operator->() is called (accessing members)
     * - operator*() is called (dereferencing)
     * - Get() is called
     *
     * Memory layout:
     * ┌─────────────────────────────────────────────────────────────────┐
     * │ m_ptr: T* - pointer to component data                            │
     * │ m_ticks: ComponentTicks* - pointer to component's ticks          │
     * │ m_currentTick: Tick - world's current tick for marking          │
     * └─────────────────────────────────────────────────────────────────┘
     *
     * Performance characteristics:
     * - Access: O(1) with minimal overhead (tick assignment on access)
     * - Change marking is deferred until actual access
     *
     * Example:
     * @code
     *   world.Query<Mut<Position>>().Each([](Mut<Position> pos) {
     *       if (some_condition) {
     *           pos->x += 1.0f;  // Automatically marks as changed
     *       }
     *       // If we don't access pos, it's not marked as changed
     *   });
     * @endcode
     */
    template <typename T> class Mut
    {
    public:
        using ComponentType = T;

        constexpr Mut() noexcept
            : m_ptr{nullptr}
            , m_ticks{nullptr}
            , m_currentTick{0}
        {
        }

        constexpr Mut(T* ptr, ComponentTicks* ticks, Tick current_tick) noexcept
            : m_ptr{ptr}
            , m_ticks{ticks}
            , m_currentTick{current_tick}
        {
        }

        /**
         * Access component through arrow operator
         * Marks the component as changed at current_tick
         */
        [[nodiscard]] T* operator->() noexcept
        {
            MarkChanged();
            return m_ptr;
        }

        [[nodiscard]] const T* operator->() const noexcept
        {
            return m_ptr;
        }

        /**
         * Dereference to get component reference
         * Marks the component as changed at current_tick
         */
        [[nodiscard]] T& operator*() noexcept
        {
            MarkChanged();
            return *m_ptr;
        }

        [[nodiscard]] const T& operator*() const noexcept
        {
            return *m_ptr;
        }

        /**
         * Get raw pointer (marks as changed)
         */
        [[nodiscard]] T* Get() noexcept
        {
            MarkChanged();
            return m_ptr;
        }

        /**
         * Get raw pointer (read-only, does not mark as changed)
         */
        [[nodiscard]] const T* Get() const noexcept
        {
            return m_ptr;
        }

        /**
         * Get raw pointer without marking as changed
         * Use when you need to read without triggering change detection
         */
        [[nodiscard]] const T* GetReadOnly() const noexcept
        {
            return m_ptr;
        }

        /**
         * Check if the wrapper holds a valid pointer
         */
        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return m_ptr != nullptr;
        }

        /**
         * Explicitly mark as changed
         * Useful when you modify through a copied reference
         */
        void MarkChanged() noexcept
        {
            if (m_ticks != nullptr)
            {
                m_ticks->MarkChanged(m_currentTick);
            }
        }

        /**
         * Check if this component was added since last_run
         */
        [[nodiscard]] bool WasAdded(Tick last_run) const noexcept
        {
            return m_ticks != nullptr && m_ticks->WasAdded(last_run);
        }

        /**
         * Check if this component was changed since last_run
         */
        [[nodiscard]] bool WasChanged(Tick last_run) const noexcept
        {
            return m_ticks != nullptr && m_ticks->WasChanged(last_run);
        }

        /**
         * Get the component's ticks (for advanced use)
         */
        [[nodiscard]] const ComponentTicks* Ticks() const noexcept
        {
            return m_ticks;
        }

    private:
        T* m_ptr;
        ComponentTicks* m_ticks;
        Tick m_currentTick;
    };

    // Type traits to detect Mut<T>
    namespace detail
    {
        template <typename T> struct IsMut : std::false_type
        {
        };

        template <typename T> struct IsMut<Mut<T>> : std::true_type
        {
        };

        template <typename T> constexpr bool IsMutV = IsMut<T>::value;

        template <typename T> struct UnwrapMut
        {
            using type = T;
        };

        template <typename T> struct UnwrapMut<Mut<T>>
        {
            using type = T;
        };

        template <typename T> using UnwrapMutT = typename UnwrapMut<T>::type;
    } // namespace detail
} // namespace queen
