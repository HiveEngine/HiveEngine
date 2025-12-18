#pragma once

#include <queen/event/event.h>
#include <queen/event/event_queue.h>
#include <queen/event/event_writer.h>
#include <queen/event/event_reader.h>
#include <comb/allocator_concepts.h>
#include <wax/containers/hash_map.h>
#include <wax/containers/vector.h>
#include <hive/core/assert.h>

namespace queen
{
    /**
     * World-owned registry of all event queues
     *
     * Events manages type-erased storage of EventQueue<T> instances.
     * Event queues are lazily created on first access. Each event type
     * gets its own double-buffered queue.
     *
     * Lifecycle:
     * - Created by World at construction
     * - Queues created lazily on first Writer/Reader access
     * - SwapBuffers() called at end of each Update()
     * - Destroyed with World
     *
     * Use cases:
     * - Central event queue management for World
     * - Type-safe access to event writers and readers
     * - Automatic buffer swapping at frame boundaries
     *
     * Memory layout:
     * ┌────────────────────────────────────────────────────────────────┐
     * │ allocator_: Allocator* (8 bytes)                               │
     * │ queues_: HashMap<TypeId, QueueEntry*>                          │
     * │   TypeId(DamageEvent) → QueueEntry { queue, swap_fn, dtor }    │
     * │   TypeId(SpawnEvent)  → QueueEntry { queue, swap_fn, dtor }    │
     * │ entries_: Vector<QueueEntry> (owns all entries)                │
     * └────────────────────────────────────────────────────────────────┘
     *
     * Performance characteristics:
     * - GetQueue: O(1) average (hash map lookup + lazy creation)
     * - SwapBuffers: O(n) where n = number of event types
     * - First access: O(1) + queue allocation
     *
     * Limitations:
     * - Event types must satisfy Event concept
     * - Not thread-safe (external synchronization needed)
     * - Queue memory persists until Events destruction
     *
     * Example:
     * @code
     *   Events<comb::BuddyAllocator> events{allocator};
     *
     *   // Get writer for damage events
     *   auto writer = events.Writer<DamageEvent>();
     *   writer.Send(DamageEvent{target, source, 10.0f});
     *
     *   // Get reader for damage events
     *   auto reader = events.Reader<DamageEvent>();
     *   for (const auto& event : reader) {
     *       // Process event
     *   }
     *
     *   // End of frame: swap all buffers
     *   events.SwapBuffers();
     * @endcode
     */
    template<comb::Allocator Allocator>
    class Events
    {
    public:
        explicit Events(Allocator& allocator)
            : allocator_{&allocator}
            , queues_{allocator, 32}
            , entries_{allocator}
        {
        }

        ~Events()
        {
            // Call destructors on all queue entries
            for (size_t i = 0; i < entries_.Size(); ++i)
            {
                if (entries_[i].destructor != nullptr)
                {
                    entries_[i].destructor(entries_[i].queue);
                }
                allocator_->Deallocate(entries_[i].queue);
            }
        }

        Events(const Events&) = delete;
        Events& operator=(const Events&) = delete;
        Events(Events&&) = default;
        Events& operator=(Events&&) = default;

        /**
         * Get EventWriter for an event type
         *
         * Creates the queue lazily if it doesn't exist.
         *
         * @tparam T Event type (must satisfy Event concept)
         * @return EventWriter for the event type
         */
        template<Event T>
        [[nodiscard]] EventWriter<T, Allocator> Writer()
        {
            return EventWriter<T, Allocator>{GetOrCreateQueue<T>()};
        }

        /**
         * Get EventReader for an event type
         *
         * Creates the queue lazily if it doesn't exist.
         *
         * @tparam T Event type (must satisfy Event concept)
         * @return EventReader for the event type
         */
        template<Event T>
        [[nodiscard]] EventReader<T, Allocator> Reader()
        {
            return EventReader<T, Allocator>{GetOrCreateQueue<T>()};
        }

        /**
         * Send an event (convenience method)
         *
         * @tparam T Event type
         * @param event Event to send
         */
        template<Event T>
        void Send(const T& event)
        {
            GetOrCreateQueue<T>().Push(event);
        }

        /**
         * Send an event (move, convenience method)
         *
         * @tparam T Event type
         * @param event Event to send
         */
        template<Event T>
        void Send(T&& event)
        {
            GetOrCreateQueue<T>().Push(std::move(event));
        }

        /**
         * Swap buffers on all event queues
         *
         * Should be called at the end of each frame (Update).
         * Clears previous frame's events and rotates buffers.
         */
        void SwapBuffers()
        {
            for (size_t i = 0; i < entries_.Size(); ++i)
            {
                if (entries_[i].swap_fn != nullptr)
                {
                    entries_[i].swap_fn(entries_[i].queue);
                }
            }
        }

        /**
         * Clear all events from all queues
         */
        void ClearAll()
        {
            for (size_t i = 0; i < entries_.Size(); ++i)
            {
                if (entries_[i].clear_fn != nullptr)
                {
                    entries_[i].clear_fn(entries_[i].queue);
                }
            }
        }

        /**
         * Check if a queue exists for an event type
         *
         * @tparam T Event type
         * @return true if queue exists
         */
        template<Event T>
        [[nodiscard]] bool HasQueue() const
        {
            TypeId id = TypeIdOf<T>();
            return queues_.Find(id) != nullptr;
        }

        /**
         * Get number of registered event types
         */
        [[nodiscard]] size_t QueueCount() const noexcept
        {
            return entries_.Size();
        }

    private:
        struct QueueEntry
        {
            void* queue;
            void (*swap_fn)(void*);
            void (*clear_fn)(void*);
            void (*destructor)(void*);
            TypeId type_id;
        };

        template<Event T>
        EventQueue<T, Allocator>& GetOrCreateQueue()
        {
            TypeId id = TypeIdOf<T>();

            // Try to find existing queue
            if (auto* entry = queues_.Find(id))
            {
                return *static_cast<EventQueue<T, Allocator>*>((*entry)->queue);
            }

            // Create new queue
            void* memory = allocator_->Allocate(sizeof(EventQueue<T, Allocator>), alignof(EventQueue<T, Allocator>));
            hive::Assert(memory != nullptr, "Failed to allocate EventQueue");

            auto* queue = new (memory) EventQueue<T, Allocator>(*allocator_);

            QueueEntry entry{};
            entry.queue = queue;
            entry.swap_fn = [](void* q) {
                static_cast<EventQueue<T, Allocator>*>(q)->Swap();
            };
            entry.clear_fn = [](void* q) {
                static_cast<EventQueue<T, Allocator>*>(q)->Clear();
            };
            entry.destructor = [](void* q) {
                static_cast<EventQueue<T, Allocator>*>(q)->~EventQueue();
            };
            entry.type_id = id;

            entries_.PushBack(entry);
            queues_.Insert(id, &entries_.Back());

            return *queue;
        }

        Allocator* allocator_;
        wax::HashMap<TypeId, QueueEntry*, Allocator> queues_;
        wax::Vector<QueueEntry, Allocator> entries_;
    };
}
