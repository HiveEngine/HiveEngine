
#include <larvae/larvae.h>
#include <queen/event/event.h>
#include <queen/event/event_queue.h>
#include <queen/event/event_writer.h>
#include <queen/event/event_reader.h>
#include <queen/event/events.h>
#include <comb/buddy_allocator.h>

namespace
{
    // Test event types
    struct DamageEvent
    {
        uint32_t target_id;
        uint32_t source_id;
        float amount;
    };

    struct SpawnEvent
    {
        float x, y, z;
        uint32_t type;
    };

    struct JumpEvent
    {
        uint32_t entity_id;
        float force;
    };

    // Verify Event concept
    static_assert(queen::Event<DamageEvent>);
    static_assert(queen::Event<SpawnEvent>);
    static_assert(queen::Event<JumpEvent>);

    // ─────────────────────────────────────────────────────────────
    // Event Concept Tests
    // ─────────────────────────────────────────────────────────────

    auto test_event_1 = larvae::RegisterTest("QueenEvent", "EventConceptValid", []() {
        static_assert(queen::Event<DamageEvent>);
        static_assert(queen::Event<SpawnEvent>);

        // Empty structs should not be events (no data to carry)
        struct EmptyEvent {};
        static_assert(!queen::Event<EmptyEvent>);
    });

    auto test_event_2 = larvae::RegisterTest("QueenEvent", "EventIdUnique", []() {
        queen::EventId damage_id = queen::EventIdOf<DamageEvent>();
        queen::EventId spawn_id = queen::EventIdOf<SpawnEvent>();
        queen::EventId damage_id2 = queen::EventIdOf<DamageEvent>();

        larvae::AssertTrue(damage_id.IsValid());
        larvae::AssertTrue(spawn_id.IsValid());
        larvae::AssertNotEqual(damage_id.Value(), spawn_id.Value());
        larvae::AssertEqual(damage_id.Value(), damage_id2.Value());
    });

    auto test_event_3 = larvae::RegisterTest("QueenEvent", "EventMetaCorrect", []() {
        queen::EventMeta damage_meta = queen::EventMeta::Of<DamageEvent>();

        larvae::AssertEqual(damage_meta.size, sizeof(DamageEvent));
        larvae::AssertEqual(damage_meta.alignment, alignof(DamageEvent));
        larvae::AssertTrue(damage_meta.id.IsValid());
    });

    // ─────────────────────────────────────────────────────────────
    // EventQueue Tests
    // ─────────────────────────────────────────────────────────────

    auto test_queue_1 = larvae::RegisterTest("QueenEvent", "EventQueueConstruction", []() {
        comb::BuddyAllocator alloc{1024 * 1024};
        queen::EventQueue<DamageEvent, comb::BuddyAllocator> queue{alloc};

        larvae::AssertTrue(queue.IsEmpty());
        larvae::AssertEqual(queue.TotalCount(), size_t{0});
        larvae::AssertEqual(queue.CurrentCount(), size_t{0});
        larvae::AssertEqual(queue.PreviousCount(), size_t{0});
    });

    auto test_queue_2 = larvae::RegisterTest("QueenEvent", "EventQueuePush", []() {
        comb::BuddyAllocator alloc{1024 * 1024};
        queen::EventQueue<DamageEvent, comb::BuddyAllocator> queue{alloc};

        queue.Push(DamageEvent{1, 2, 10.0f});
        queue.Push(DamageEvent{3, 4, 20.0f});

        larvae::AssertFalse(queue.IsEmpty());
        larvae::AssertEqual(queue.CurrentCount(), size_t{2});
        larvae::AssertEqual(queue.PreviousCount(), size_t{0});
        larvae::AssertEqual(queue.TotalCount(), size_t{2});
    });

    auto test_queue_3 = larvae::RegisterTest("QueenEvent", "EventQueueEmplace", []() {
        comb::BuddyAllocator alloc{1024 * 1024};
        queen::EventQueue<DamageEvent, comb::BuddyAllocator> queue{alloc};

        auto& event = queue.Emplace(1u, 2u, 15.0f);

        larvae::AssertEqual(event.target_id, 1u);
        larvae::AssertEqual(event.source_id, 2u);
        larvae::AssertEqual(event.amount, 15.0f);
        larvae::AssertEqual(queue.CurrentCount(), size_t{1});
    });

    auto test_queue_4 = larvae::RegisterTest("QueenEvent", "EventQueueSwap", []() {
        comb::BuddyAllocator alloc{1024 * 1024};
        queen::EventQueue<DamageEvent, comb::BuddyAllocator> queue{alloc};

        // Frame 1: Add 2 events
        queue.Push(DamageEvent{1, 2, 10.0f});
        queue.Push(DamageEvent{3, 4, 20.0f});

        larvae::AssertEqual(queue.CurrentCount(), size_t{2});
        larvae::AssertEqual(queue.PreviousCount(), size_t{0});

        // Swap: current becomes previous
        queue.Swap();

        larvae::AssertEqual(queue.CurrentCount(), size_t{0});
        larvae::AssertEqual(queue.PreviousCount(), size_t{2});
        larvae::AssertEqual(queue.TotalCount(), size_t{2});

        // Frame 2: Add 1 event
        queue.Push(DamageEvent{5, 6, 30.0f});

        larvae::AssertEqual(queue.CurrentCount(), size_t{1});
        larvae::AssertEqual(queue.PreviousCount(), size_t{2});
        larvae::AssertEqual(queue.TotalCount(), size_t{3});

        // Swap again: previous (2 events) cleared
        queue.Swap();

        larvae::AssertEqual(queue.CurrentCount(), size_t{0});
        larvae::AssertEqual(queue.PreviousCount(), size_t{1});
        larvae::AssertEqual(queue.TotalCount(), size_t{1});
    });

    auto test_queue_5 = larvae::RegisterTest("QueenEvent", "EventQueueIteration", []() {
        comb::BuddyAllocator alloc{1024 * 1024};
        queen::EventQueue<DamageEvent, comb::BuddyAllocator> queue{alloc};

        queue.Push(DamageEvent{1, 0, 10.0f});
        queue.Push(DamageEvent{2, 0, 20.0f});
        queue.Swap();
        queue.Push(DamageEvent{3, 0, 30.0f});

        // Should iterate: previous first (1, 2), then current (3)
        float total = 0.0f;
        size_t count = 0;
        for (const auto& event : queue)
        {
            total += event.amount;
            ++count;
        }

        larvae::AssertEqual(count, size_t{3});
        larvae::AssertEqual(total, 60.0f);
    });

    auto test_queue_6 = larvae::RegisterTest("QueenEvent", "EventQueueClear", []() {
        comb::BuddyAllocator alloc{1024 * 1024};
        queen::EventQueue<DamageEvent, comb::BuddyAllocator> queue{alloc};

        queue.Push(DamageEvent{1, 2, 10.0f});
        queue.Swap();
        queue.Push(DamageEvent{3, 4, 20.0f});

        larvae::AssertFalse(queue.IsEmpty());

        queue.Clear();

        larvae::AssertTrue(queue.IsEmpty());
        larvae::AssertEqual(queue.TotalCount(), size_t{0});
    });

    // ─────────────────────────────────────────────────────────────
    // EventWriter Tests
    // ─────────────────────────────────────────────────────────────

    auto test_writer_1 = larvae::RegisterTest("QueenEvent", "EventWriterSend", []() {
        comb::BuddyAllocator alloc{1024 * 1024};
        queen::EventQueue<DamageEvent, comb::BuddyAllocator> queue{alloc};
        queen::EventWriter<DamageEvent, comb::BuddyAllocator> writer{queue};

        writer.Send(DamageEvent{1, 2, 10.0f});
        writer.Send(DamageEvent{3, 4, 20.0f});

        larvae::AssertEqual(writer.Count(), size_t{2});
        larvae::AssertFalse(writer.IsEmpty());
    });

    auto test_writer_2 = larvae::RegisterTest("QueenEvent", "EventWriterEmplace", []() {
        comb::BuddyAllocator alloc{1024 * 1024};
        queen::EventQueue<DamageEvent, comb::BuddyAllocator> queue{alloc};
        queen::EventWriter<DamageEvent, comb::BuddyAllocator> writer{queue};

        auto& event = writer.Emplace(1u, 2u, 15.0f);

        larvae::AssertEqual(event.target_id, 1u);
        larvae::AssertEqual(writer.Count(), size_t{1});
    });

    // ─────────────────────────────────────────────────────────────
    // EventReader Tests
    // ─────────────────────────────────────────────────────────────

    auto test_reader_1 = larvae::RegisterTest("QueenEvent", "EventReaderIterate", []() {
        comb::BuddyAllocator alloc{1024 * 1024};
        queen::EventQueue<DamageEvent, comb::BuddyAllocator> queue{alloc};

        queue.Push(DamageEvent{1, 0, 10.0f});
        queue.Push(DamageEvent{2, 0, 20.0f});

        queen::EventReader<DamageEvent, comb::BuddyAllocator> reader{queue};

        larvae::AssertEqual(reader.Count(), size_t{2});
        larvae::AssertFalse(reader.IsEmpty());

        float total = 0.0f;
        for (const auto& event : reader)
        {
            total += event.amount;
        }

        larvae::AssertEqual(total, 30.0f);
    });

    auto test_reader_2 = larvae::RegisterTest("QueenEvent", "EventReaderRead", []() {
        comb::BuddyAllocator alloc{1024 * 1024};
        queen::EventQueue<DamageEvent, comb::BuddyAllocator> queue{alloc};

        queue.Push(DamageEvent{1, 0, 10.0f});
        queue.Push(DamageEvent{2, 0, 20.0f});

        queen::EventReader<DamageEvent, comb::BuddyAllocator> reader{queue};

        float total = 0.0f;
        reader.Read([&total](const DamageEvent& event) {
            total += event.amount;
        });

        larvae::AssertEqual(total, 30.0f);
        larvae::AssertTrue(reader.IsEmpty());  // Cursor advanced
    });

    auto test_reader_3 = larvae::RegisterTest("QueenEvent", "EventReaderMarkRead", []() {
        comb::BuddyAllocator alloc{1024 * 1024};
        queen::EventQueue<DamageEvent, comb::BuddyAllocator> queue{alloc};

        queue.Push(DamageEvent{1, 0, 10.0f});

        queen::EventReader<DamageEvent, comb::BuddyAllocator> reader{queue};

        larvae::AssertFalse(reader.IsEmpty());

        reader.MarkRead();

        larvae::AssertTrue(reader.IsEmpty());
        larvae::AssertEqual(reader.Count(), size_t{0});
    });

    auto test_reader_4 = larvae::RegisterTest("QueenEvent", "EventReaderReset", []() {
        comb::BuddyAllocator alloc{1024 * 1024};
        queen::EventQueue<DamageEvent, comb::BuddyAllocator> queue{alloc};

        queue.Push(DamageEvent{1, 0, 10.0f});

        queen::EventReader<DamageEvent, comb::BuddyAllocator> reader{queue};

        reader.MarkRead();
        larvae::AssertTrue(reader.IsEmpty());

        reader.Reset();
        larvae::AssertFalse(reader.IsEmpty());
        larvae::AssertEqual(reader.Count(), size_t{1});
    });

    // ─────────────────────────────────────────────────────────────
    // Events Registry Tests
    // ─────────────────────────────────────────────────────────────

    auto test_events_1 = larvae::RegisterTest("QueenEvent", "EventsConstruction", []() {
        comb::BuddyAllocator alloc{1024 * 1024};
        queen::Events<comb::BuddyAllocator> events{alloc};

        larvae::AssertEqual(events.QueueCount(), size_t{0});
    });

    auto test_events_2 = larvae::RegisterTest("QueenEvent", "EventsLazyCreation", []() {
        comb::BuddyAllocator alloc{1024 * 1024};
        queen::Events<comb::BuddyAllocator> events{alloc};

        larvae::AssertFalse(events.HasQueue<DamageEvent>());

        auto writer = events.Writer<DamageEvent>();
        (void)writer;

        larvae::AssertTrue(events.HasQueue<DamageEvent>());
        larvae::AssertEqual(events.QueueCount(), size_t{1});
    });

    auto test_events_3 = larvae::RegisterTest("QueenEvent", "EventsMultipleTypes", []() {
        comb::BuddyAllocator alloc{1024 * 1024};
        queen::Events<comb::BuddyAllocator> events{alloc};

        events.Send(DamageEvent{1, 2, 10.0f});
        events.Send(SpawnEvent{1.0f, 2.0f, 3.0f, 1});
        events.Send(JumpEvent{1, 5.0f});

        larvae::AssertEqual(events.QueueCount(), size_t{3});
        larvae::AssertTrue(events.HasQueue<DamageEvent>());
        larvae::AssertTrue(events.HasQueue<SpawnEvent>());
        larvae::AssertTrue(events.HasQueue<JumpEvent>());
    });

    auto test_events_4 = larvae::RegisterTest("QueenEvent", "EventsWriterReader", []() {
        comb::BuddyAllocator alloc{1024 * 1024};
        queen::Events<comb::BuddyAllocator> events{alloc};

        auto writer = events.Writer<DamageEvent>();
        writer.Send(DamageEvent{1, 2, 10.0f});
        writer.Send(DamageEvent{3, 4, 20.0f});

        auto reader = events.Reader<DamageEvent>();
        larvae::AssertEqual(reader.TotalCount(), size_t{2});

        float total = 0.0f;
        for (const auto& event : reader)
        {
            total += event.amount;
        }

        larvae::AssertEqual(total, 30.0f);
    });

    auto test_events_5 = larvae::RegisterTest("QueenEvent", "EventsSwapBuffers", []() {
        comb::BuddyAllocator alloc{1024 * 1024};
        queen::Events<comb::BuddyAllocator> events{alloc};

        // Frame 1
        events.Send(DamageEvent{1, 2, 10.0f});
        events.Send(DamageEvent{3, 4, 20.0f});

        auto reader1 = events.Reader<DamageEvent>();
        larvae::AssertEqual(reader1.TotalCount(), size_t{2});

        events.SwapBuffers();

        // Frame 2: previous events still readable
        auto reader2 = events.Reader<DamageEvent>();
        larvae::AssertEqual(reader2.TotalCount(), size_t{2});

        events.SwapBuffers();

        // Frame 3: previous events cleared
        auto reader3 = events.Reader<DamageEvent>();
        larvae::AssertEqual(reader3.TotalCount(), size_t{0});
    });

    auto test_events_6 = larvae::RegisterTest("QueenEvent", "EventsClearAll", []() {
        comb::BuddyAllocator alloc{1024 * 1024};
        queen::Events<comb::BuddyAllocator> events{alloc};

        events.Send(DamageEvent{1, 2, 10.0f});
        events.Send(SpawnEvent{1.0f, 2.0f, 3.0f, 1});

        auto reader1 = events.Reader<DamageEvent>();
        larvae::AssertFalse(reader1.IsEmpty());

        events.ClearAll();

        auto reader2 = events.Reader<DamageEvent>();
        larvae::AssertTrue(reader2.IsEmpty());

        auto reader3 = events.Reader<SpawnEvent>();
        larvae::AssertTrue(reader3.IsEmpty());
    });

    auto test_events_7 = larvae::RegisterTest("QueenEvent", "EventsTwoFrameRetention", []() {
        comb::BuddyAllocator alloc{1024 * 1024};
        queen::Events<comb::BuddyAllocator> events{alloc};

        // Frame 1: Send event
        events.Send(DamageEvent{1, 2, 10.0f});

        // Frame 1: Readable
        {
            auto reader = events.Reader<DamageEvent>();
            larvae::AssertEqual(reader.TotalCount(), size_t{1});
        }

        events.SwapBuffers();

        // Frame 2: Still readable (in previous buffer)
        {
            auto reader = events.Reader<DamageEvent>();
            larvae::AssertEqual(reader.TotalCount(), size_t{1});
        }

        events.SwapBuffers();

        // Frame 3: Cleared (beyond 2-frame retention)
        {
            auto reader = events.Reader<DamageEvent>();
            larvae::AssertEqual(reader.TotalCount(), size_t{0});
        }
    });
}
