#pragma once

#include <queen/event/event.h>
#include <comb/allocator_concepts.h>
#include <wax/containers/vector.h>
#include <cstddef>

namespace queen
{
    /**
     * Double-buffered event queue for frame-safe event processing
     *
     * Events written in frame N are readable in frames N and N+1.
     * After frame N+1, events are silently dropped (prevents leaks).
     * This allows systems to read events regardless of execution order.
     *
     * The double-buffer design ensures:
     * - Writers never invalidate active readers
     * - Events persist for exactly 2 frames
     * - No explicit cleanup required (automatic on swap)
     *
     * Use cases:
     * - Gameplay event communication between systems
     * - Input event distribution
     * - Deferred reactions to game state changes
     * - Audio/VFX triggers from gameplay logic
     *
     * Memory layout:
     * ┌────────────────────────────────────────────────────────────────┐
     * │ current_: uint8_t (buffer index 0 or 1)                        │
     * │ buffers_[2]: Vector<T> (double-buffered event storage)         │
     * │                                                                │
     * │ Frame N:                                                       │
     * │ ┌─────────────────────────┬─────────────────────────┐          │
     * │ │ buffers_[current_]      │ buffers_[1-current_]    │          │
     * │ │ [E1][E2][E3] ← writes   │ [E0] ← from frame N-1   │          │
     * │ │ (current frame events)  │ (previous frame events) │          │
     * │ └─────────────────────────┴─────────────────────────┘          │
     * │                                                                │
     * │ After Swap():                                                  │
     * │ - Previous buffer cleared                                      │
     * │ - Current becomes previous                                     │
     * │ - current_ index flipped                                       │
     * └────────────────────────────────────────────────────────────────┘
     *
     * Performance characteristics:
     * - Write (Push): O(1) amortized - vector push_back
     * - Read iteration: O(n) - cache-friendly sequential access
     * - Swap: O(1) - index flip + vector clear
     * - Memory: Contiguous storage per buffer (cache-friendly)
     * - Thread-safe: No (use external synchronization)
     *
     * Limitations:
     * - Events must satisfy Event concept (trivially copyable)
     * - 2-frame maximum lifetime (silent drop after)
     * - Not thread-safe for concurrent writes
     * - Single writer expected (use per-system queues for parallel)
     *
     * Example:
     * @code
     *   comb::BuddyAllocator alloc{1_MB};
     *   EventQueue<DamageEvent, comb::BuddyAllocator> queue{alloc};
     *
     *   // Frame 1: Write events
     *   queue.Push(DamageEvent{target, source, 10.0f});
     *   queue.Push(DamageEvent{target2, source, 5.0f});
     *
     *   // Read events (current + previous frame)
     *   for (const auto& event : queue) {
     *       ApplyDamage(event);
     *   }
     *
     *   // End of frame: swap buffers
     *   queue.Swap();  // Previous cleared, current becomes previous
     *
     *   // Frame 2: Events from frame 1 still readable
     *   for (const auto& event : queue) {
     *       // Still sees frame 1 events
     *   }
     *
     *   queue.Swap();  // Frame 1 events now cleared
     * @endcode
     */
    template<Event T, comb::Allocator Allocator>
    class EventQueue
    {
    public:
        using EventType = T;
        using Iterator = const T*;

        explicit EventQueue(Allocator& allocator)
            : buffers_{wax::Vector<T, Allocator>{allocator}, wax::Vector<T, Allocator>{allocator}}
            , current_{0}
        {
        }

        ~EventQueue() = default;

        EventQueue(const EventQueue&) = delete;
        EventQueue& operator=(const EventQueue&) = delete;
        EventQueue(EventQueue&&) = default;
        EventQueue& operator=(EventQueue&&) = default;

        /**
         * Add an event to the current frame's buffer
         *
         * @param event Event to add (copied into buffer)
         */
        void Push(const T& event)
        {
            buffers_[current_].PushBack(event);
        }

        /**
         * Add an event to the current frame's buffer (move)
         *
         * @param event Event to add (moved into buffer)
         */
        void Push(T&& event)
        {
            buffers_[current_].PushBack(std::move(event));
        }

        /**
         * Construct an event in-place in the current frame's buffer
         *
         * @tparam Args Constructor argument types
         * @param args Arguments forwarded to event constructor
         * @return Reference to the constructed event
         */
        template<typename... Args>
        T& Emplace(Args&&... args)
        {
            return buffers_[current_].EmplaceBack(std::forward<Args>(args)...);
        }

        /**
         * Swap buffers at end of frame
         *
         * - Clears the previous frame's buffer (now 2 frames old)
         * - Current buffer becomes previous
         * - New current buffer is empty (just cleared)
         */
        void Swap()
        {
            // Flip to the other buffer (which contains old events)
            current_ = 1 - current_;
            // Clear the new current buffer (was previous, now will be current)
            buffers_[current_].Clear();
        }

        /**
         * Clear all events from both buffers
         */
        void Clear()
        {
            buffers_[0].Clear();
            buffers_[1].Clear();
        }

        /**
         * Get number of events in current frame's buffer
         */
        [[nodiscard]] size_t CurrentCount() const noexcept
        {
            return buffers_[current_].Size();
        }

        /**
         * Get number of events in previous frame's buffer
         */
        [[nodiscard]] size_t PreviousCount() const noexcept
        {
            return buffers_[1 - current_].Size();
        }

        /**
         * Get total number of events across both buffers
         */
        [[nodiscard]] size_t TotalCount() const noexcept
        {
            return buffers_[0].Size() + buffers_[1].Size();
        }

        /**
         * Check if there are no events in either buffer
         */
        [[nodiscard]] bool IsEmpty() const noexcept
        {
            return buffers_[0].IsEmpty() && buffers_[1].IsEmpty();
        }

        /**
         * Check if current frame buffer is empty
         */
        [[nodiscard]] bool IsCurrentEmpty() const noexcept
        {
            return buffers_[current_].IsEmpty();
        }

        // ─────────────────────────────────────────────────────────────
        // Iteration (reads both buffers: previous first, then current)
        // ─────────────────────────────────────────────────────────────

        /**
         * Iterator for reading events from both buffers
         *
         * Iterates previous frame events first, then current frame events.
         * This ensures events are processed in chronological order.
         */
        class EventIterator
        {
        public:
            EventIterator(const EventQueue* queue, size_t index)
                : queue_{queue}
                , index_{index}
            {
            }

            const T& operator*() const
            {
                size_t prev_size = queue_->PreviousCount();
                if (index_ < prev_size)
                {
                    return queue_->buffers_[1 - queue_->current_][index_];
                }
                return queue_->buffers_[queue_->current_][index_ - prev_size];
            }

            const T* operator->() const
            {
                return &(**this);
            }

            EventIterator& operator++()
            {
                ++index_;
                return *this;
            }

            bool operator==(const EventIterator& other) const
            {
                return index_ == other.index_;
            }

            bool operator!=(const EventIterator& other) const
            {
                return index_ != other.index_;
            }

        private:
            const EventQueue* queue_;
            size_t index_;
        };

        [[nodiscard]] EventIterator begin() const
        {
            return EventIterator{this, 0};
        }

        [[nodiscard]] EventIterator end() const
        {
            return EventIterator{this, TotalCount()};
        }

        // ─────────────────────────────────────────────────────────────
        // Direct buffer access (for advanced use)
        // ─────────────────────────────────────────────────────────────

        /**
         * Get read-only access to current frame's events
         */
        [[nodiscard]] const wax::Vector<T, Allocator>& CurrentBuffer() const noexcept
        {
            return buffers_[current_];
        }

        /**
         * Get read-only access to previous frame's events
         */
        [[nodiscard]] const wax::Vector<T, Allocator>& PreviousBuffer() const noexcept
        {
            return buffers_[1 - current_];
        }

    private:
        wax::Vector<T, Allocator> buffers_[2];
        uint8_t current_;
    };
}
