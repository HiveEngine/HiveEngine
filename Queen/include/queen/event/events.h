#pragma once

#include <hive/core/assert.h>

#include <comb/allocator_concepts.h>

#include <wax/containers/hash_map.h>
#include <wax/containers/vector.h>

#include <queen/event/event.h>
#include <queen/event/event_queue.h>
#include <queen/event/event_reader.h>
#include <queen/event/event_writer.h>

#include <type_traits>
#include <utility>

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
    template <comb::Allocator Allocator> class Events
    {
    public:
        explicit Events(Allocator& allocator)
            : m_allocator{&allocator}
            , m_queues{allocator, 32}
            , m_entries{allocator}
        {
        }

        ~Events()
        {
            // Call destructors on all queue entries
            for (size_t i = 0; i < m_entries.Size(); ++i)
            {
                if (m_entries[i].m_destructor != nullptr)
                {
                    m_entries[i].m_destructor(m_entries[i].m_queue);
                }
                m_allocator->Deallocate(m_entries[i].m_queue);
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
        template <Event T> [[nodiscard]] EventWriter<T, Allocator> Writer()
        {
            return EventWriter<T, Allocator>{this->template GetOrCreateQueue<T>()};
        }

        /**
         * Get EventReader for an event type
         *
         * Creates the queue lazily if it doesn't exist.
         *
         * @tparam T Event type (must satisfy Event concept)
         * @return EventReader for the event type
         */
        template <Event T> [[nodiscard]] EventReader<T, Allocator> Reader()
        {
            return EventReader<T, Allocator>{this->template GetOrCreateQueue<T>()};
        }

        /**
         * Send an event (convenience method)
         *
         * @tparam T Event type
         * @param event Event to send
         */
        template <Event T> void Send(const T& event)
        {
            this->template GetOrCreateQueue<T>().Push(event);
        }

        /**
         * Send an event (move, convenience method)
         *
         * @tparam T Event type
         * @param event Event to send
         */
        template <Event T> void Send(T&& event)
        {
            using EventType = std::remove_cvref_t<T>;
            this->template GetOrCreateQueue<EventType>().Push(std::forward<T>(event));
        }

        /**
         * Swap buffers on all event queues
         *
         * Should be called at the end of each frame (Update).
         * Clears previous frame's events and rotates buffers.
         */
        void SwapBuffers()
        {
            for (size_t i = 0; i < m_entries.Size(); ++i)
            {
                if (m_entries[i].m_swapFn != nullptr)
                {
                    m_entries[i].m_swapFn(m_entries[i].m_queue);
                }
            }
        }

        /**
         * Clear all events from all queues
         */
        void ClearAll()
        {
            for (size_t i = 0; i < m_entries.Size(); ++i)
            {
                if (m_entries[i].m_clearFn != nullptr)
                {
                    m_entries[i].m_clearFn(m_entries[i].m_queue);
                }
            }
        }

        /**
         * Check if a queue exists for an event type
         *
         * @tparam T Event type
         * @return true if queue exists
         */
        template <Event T> [[nodiscard]] bool HasQueue() const
        {
            TypeId id = TypeIdOf<T>();
            return m_queues.Find(id) != nullptr;
        }

        /**
         * Get number of registered event types
         */
        [[nodiscard]] size_t QueueCount() const noexcept
        {
            return m_entries.Size();
        }

    private:
        struct QueueEntry
        {
            void* m_queue;
            void (*m_swapFn)(void*);
            void (*m_clearFn)(void*);
            void (*m_destructor)(void*);
            TypeId m_typeId;
        };

        template <Event T> EventQueue<T, Allocator>& GetOrCreateQueue()
        {
            TypeId id = TypeIdOf<T>();

            if (auto* index = m_queues.Find(id))
            {
                return *static_cast<EventQueue<T, Allocator>*>(m_entries[*index].m_queue);
            }

            void* memory = m_allocator->Allocate(sizeof(EventQueue<T, Allocator>), alignof(EventQueue<T, Allocator>));
            hive::Assert(memory != nullptr, "Failed to allocate EventQueue");

            auto* queue = new (memory) EventQueue<T, Allocator>(*m_allocator);

            QueueEntry entry{};
            entry.m_queue = queue;
            entry.m_swapFn = [](void* q) {
                static_cast<EventQueue<T, Allocator>*>(q)->Swap();
            };
            entry.m_clearFn = [](void* q) {
                static_cast<EventQueue<T, Allocator>*>(q)->Clear();
            };
            entry.m_destructor = [](void* q) {
                static_cast<EventQueue<T, Allocator>*>(q)->~EventQueue();
            };
            entry.m_typeId = id;

            size_t newIndex = m_entries.Size();
            m_entries.PushBack(entry);
            m_queues.Insert(id, newIndex);

            return *queue;
        }

        Allocator* m_allocator;
        wax::HashMap<TypeId, size_t> m_queues;
        wax::Vector<QueueEntry> m_entries;
    };
} // namespace queen
