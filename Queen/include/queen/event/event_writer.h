#pragma once

#include <queen/event/event.h>
#include <queen/event/event_queue.h>
#include <comb/allocator_concepts.h>

namespace queen
{
    /**
     * Write handle for sending events to an EventQueue
     *
     * EventWriter provides a focused interface for sending events.
     * It wraps an EventQueue reference and exposes only write operations.
     * Used as a system parameter for type-safe event emission.
     *
     * Use cases:
     * - System parameter for sending gameplay events
     * - Deferred event emission during query iteration
     * - Batch event sending with reserve optimization
     * - Type-safe event emission API
     *
     * Memory layout:
     * ┌────────────────────────────────────────┐
     * │ queue_: EventQueue<T>* (8 bytes)       │
     * └────────────────────────────────────────┘
     *
     * Performance characteristics:
     * - Send: O(1) amortized - delegates to queue Push
     * - SendBatch: O(n) - n push operations
     * - Construction: O(1) - pointer assignment
     * - No allocations (just a view over queue)
     *
     * Limitations:
     * - Not thread-safe (serialize writes externally)
     * - Requires valid queue reference for lifetime
     * - Write-only (cannot read events)
     *
     * Example:
     * @code
     *   // As system parameter
     *   world.System<Read<Input>>("InputSystem")
     *       .Run([](EventWriter<JumpEvent>& writer, Query<Read<Input>>& q) {
     *           q.Each([&](const Input& input) {
     *               if (input.jump_pressed) {
     *                   writer.Send(JumpEvent{input.entity, 10.0f});
     *               }
     *           });
     *       });
     *
     *   // Batch sending with reserve
     *   writer.Reserve(100);
     *   for (auto& enemy : enemies) {
     *       writer.Send(SpawnEvent{enemy.position});
     *   }
     * @endcode
     */
    template<Event T, comb::Allocator Allocator>
    class EventWriter
    {
    public:
        using EventType = T;

        explicit EventWriter(EventQueue<T, Allocator>& queue) noexcept
            : queue_{&queue}
        {
        }

        /**
         * Send an event (copy)
         *
         * @param event Event to send
         */
        void Send(const T& event)
        {
            queue_->Push(event);
        }

        /**
         * Send an event (move)
         *
         * @param event Event to send
         */
        void Send(T&& event)
        {
            queue_->Push(std::move(event));
        }

        /**
         * Construct and send an event in-place
         *
         * @tparam Args Constructor argument types
         * @param args Arguments forwarded to event constructor
         * @return Reference to the sent event
         */
        template<typename... Args>
        T& Emplace(Args&&... args)
        {
            return queue_->Emplace(std::forward<Args>(args)...);
        }

        /**
         * Send multiple events from a range
         *
         * @tparam InputIt Iterator type
         * @param first Begin iterator
         * @param last End iterator
         */
        template<typename InputIt>
        void SendBatch(InputIt first, InputIt last)
        {
            for (; first != last; ++first)
            {
                queue_->Push(*first);
            }
        }

        /**
         * Get number of events sent this frame
         */
        [[nodiscard]] size_t Count() const noexcept
        {
            return queue_->CurrentCount();
        }

        /**
         * Check if no events have been sent this frame
         */
        [[nodiscard]] bool IsEmpty() const noexcept
        {
            return queue_->IsCurrentEmpty();
        }

    private:
        EventQueue<T, Allocator>* queue_;
    };
}
