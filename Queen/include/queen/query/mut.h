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
     * │ ptr_: T* - pointer to component data                            │
     * │ ticks_: ComponentTicks* - pointer to component's ticks          │
     * │ current_tick_: Tick - world's current tick for marking          │
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
    template<typename T>
    class Mut
    {
    public:
        using ComponentType = T;

        constexpr Mut() noexcept
            : ptr_{nullptr}
            , ticks_{nullptr}
            , current_tick_{0}
        {}

        constexpr Mut(T* ptr, ComponentTicks* ticks, Tick current_tick) noexcept
            : ptr_{ptr}
            , ticks_{ticks}
            , current_tick_{current_tick}
        {}

        /**
         * Access component through arrow operator
         * Marks the component as changed at current_tick
         */
        [[nodiscard]] T* operator->() noexcept
        {
            MarkChanged();
            return ptr_;
        }

        [[nodiscard]] const T* operator->() const noexcept
        {
            return ptr_;
        }

        /**
         * Dereference to get component reference
         * Marks the component as changed at current_tick
         */
        [[nodiscard]] T& operator*() noexcept
        {
            MarkChanged();
            return *ptr_;
        }

        [[nodiscard]] const T& operator*() const noexcept
        {
            return *ptr_;
        }

        /**
         * Get raw pointer (marks as changed)
         */
        [[nodiscard]] T* Get() noexcept
        {
            MarkChanged();
            return ptr_;
        }

        /**
         * Get raw pointer (read-only, does not mark as changed)
         */
        [[nodiscard]] const T* Get() const noexcept
        {
            return ptr_;
        }

        /**
         * Get raw pointer without marking as changed
         * Use when you need to read without triggering change detection
         */
        [[nodiscard]] const T* GetReadOnly() const noexcept
        {
            return ptr_;
        }

        /**
         * Check if the wrapper holds a valid pointer
         */
        [[nodiscard]] constexpr explicit operator bool() const noexcept
        {
            return ptr_ != nullptr;
        }

        /**
         * Explicitly mark as changed
         * Useful when you modify through a copied reference
         */
        void MarkChanged() noexcept
        {
            if (ticks_ != nullptr)
            {
                ticks_->MarkChanged(current_tick_);
            }
        }

        /**
         * Check if this component was added since last_run
         */
        [[nodiscard]] bool WasAdded(Tick last_run) const noexcept
        {
            return ticks_ != nullptr && ticks_->WasAdded(last_run);
        }

        /**
         * Check if this component was changed since last_run
         */
        [[nodiscard]] bool WasChanged(Tick last_run) const noexcept
        {
            return ticks_ != nullptr && ticks_->WasChanged(last_run);
        }

        /**
         * Get the component's ticks (for advanced use)
         */
        [[nodiscard]] const ComponentTicks* Ticks() const noexcept
        {
            return ticks_;
        }

    private:
        T* ptr_;
        ComponentTicks* ticks_;
        Tick current_tick_;
    };

    // Type traits to detect Mut<T>
    namespace detail
    {
        template<typename T>
        struct IsMut : std::false_type {};

        template<typename T>
        struct IsMut<Mut<T>> : std::true_type {};

        template<typename T>
        constexpr bool IsMutV = IsMut<T>::value;

        template<typename T>
        struct UnwrapMut
        {
            using type = T;
        };

        template<typename T>
        struct UnwrapMut<Mut<T>>
        {
            using type = T;
        };

        template<typename T>
        using UnwrapMutT = typename UnwrapMut<T>::type;
    }
}
