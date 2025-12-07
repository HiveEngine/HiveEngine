#pragma once

#include <queen/event/event.h>
#include <queen/event/event_queue.h>
#include <comb/allocator_concepts.h>

namespace queen
{
    /**
     * Read handle for consuming events from an EventQueue
     *
     * EventReader provides shared read access to events from both the
     * current and previous frame. It tracks a cursor position to allow
     * multiple reads without re-processing events.
     *
     * The cursor tracks how many events have been read. When new events
     * are added, they appear after the cursor position. This allows
     * systems to read only new events since their last read.
     *
     * Use cases:
     * - System parameter for reacting to gameplay events
     * - Processing batched events efficiently
     * - Multiple systems reading same event type
     * - Event filtering and transformation
     *
     * Memory layout:
     * ┌────────────────────────────────────────┐
     * │ queue_: EventQueue<T>* (8 bytes)       │
     * │ cursor_: size_t (8 bytes)              │
     * └────────────────────────────────────────┘
     *
     * Performance characteristics:
     * - Iteration: O(n) - cache-friendly sequential access
     * - IsEmpty: O(1) - cursor comparison
     * - Clear/Reset: O(1) - counter reset
     * - Construction: O(1) - pointer + counter init
     *
     * Limitations:
     * - Read-only (cannot send events)
     * - Cursor is per-reader (not shared between readers)
     * - Must reset cursor manually if re-reading is needed
     *
     * Example:
     * @code
     *   // As system parameter
     *   world.System<Write<Health>>("DamageSystem")
     *       .Run([](EventReader<DamageEvent>& reader, Query<Write<Health>>& q) {
     *           for (const auto& event : reader) {
     *               if (auto* hp = q.Get(event.target)) {
     *                   hp->value -= event.amount;
     *               }
     *           }
     *       });
     *
     *   // Check if there are unread events
     *   if (!reader.IsEmpty()) {
     *       ProcessEvents(reader);
     *   }
     *
     *   // Read count
     *   size_t count = reader.Count();
     * @endcode
     */
    template<Event T, comb::Allocator Allocator>
    class EventReader
    {
    public:
        using EventType = T;
        using Iterator = typename EventQueue<T, Allocator>::EventIterator;

        explicit EventReader(EventQueue<T, Allocator>& queue) noexcept
            : queue_{&queue}
            , cursor_{0}
        {
        }

        /**
         * Get iterator to first unread event
         */
        [[nodiscard]] Iterator begin() const
        {
            return Iterator{queue_, cursor_};
        }

        /**
         * Get iterator past last event
         */
        [[nodiscard]] Iterator end() const
        {
            return Iterator{queue_, queue_->TotalCount()};
        }

        /**
         * Iterate over unread events and advance cursor
         *
         * This is the preferred way to read events as it automatically
         * tracks which events have been processed.
         *
         * @tparam Func Callable with signature void(const T&)
         * @param func Function to call for each event
         */
        template<typename Func>
        void Read(Func&& func)
        {
            size_t total = queue_->TotalCount();
            size_t prev_size = queue_->PreviousCount();

            for (size_t i = cursor_; i < total; ++i)
            {
                if (i < prev_size)
                {
                    func(queue_->PreviousBuffer()[i]);
                }
                else
                {
                    func(queue_->CurrentBuffer()[i - prev_size]);
                }
            }

            cursor_ = total;
        }

        /**
         * Get number of unread events
         */
        [[nodiscard]] size_t Count() const noexcept
        {
            size_t total = queue_->TotalCount();
            return total > cursor_ ? total - cursor_ : 0;
        }

        /**
         * Get total number of events (read + unread)
         */
        [[nodiscard]] size_t TotalCount() const noexcept
        {
            return queue_->TotalCount();
        }

        /**
         * Check if there are no unread events
         */
        [[nodiscard]] bool IsEmpty() const noexcept
        {
            return cursor_ >= queue_->TotalCount();
        }

        /**
         * Mark all current events as read
         *
         * After calling this, IsEmpty() returns true until new events arrive.
         */
        void MarkRead() noexcept
        {
            cursor_ = queue_->TotalCount();
        }

        /**
         * Reset cursor to re-read all events
         *
         * Allows re-processing all events from both buffers.
         */
        void Reset() noexcept
        {
            cursor_ = 0;
        }

        /**
         * Clear cursor (alias for MarkRead for Bevy-like API)
         */
        void Clear() noexcept
        {
            MarkRead();
        }

    private:
        EventQueue<T, Allocator>* queue_;
        size_t cursor_;
    };
}
