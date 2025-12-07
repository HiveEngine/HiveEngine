
#include <larvae/larvae.h>
#include <queen/observer/observer_event.h>
#include <queen/observer/observer.h>
#include <queen/observer/observer_builder.h>
#include <queen/observer/observer_storage.h>
#include <queen/world/world.h>
#include <queen/core/entity.h>
#include <comb/buddy_allocator.h>

namespace
{
    // Test component types
    struct Health
    {
        float value;
        float max_value;
    };

    struct Position
    {
        float x, y, z;
    };

    struct Velocity
    {
        float dx, dy, dz;
    };

    // ─────────────────────────────────────────────────────────────
    // Observer Event Type Tests
    // ─────────────────────────────────────────────────────────────

    auto test_observer_1 = larvae::RegisterTest("QueenObserver", "OnAddTriggerTypeDetection", []() {
        static_assert(queen::IsOnAddTrigger<queen::OnAdd<Health>>);
        static_assert(!queen::IsOnAddTrigger<queen::OnRemove<Health>>);
        static_assert(!queen::IsOnAddTrigger<queen::OnSet<Health>>);
        static_assert(!queen::IsOnAddTrigger<Health>);
    });

    auto test_observer_2 = larvae::RegisterTest("QueenObserver", "OnRemoveTriggerTypeDetection", []() {
        static_assert(queen::IsOnRemoveTrigger<queen::OnRemove<Health>>);
        static_assert(!queen::IsOnRemoveTrigger<queen::OnAdd<Health>>);
        static_assert(!queen::IsOnRemoveTrigger<queen::OnSet<Health>>);
        static_assert(!queen::IsOnRemoveTrigger<Health>);
    });

    auto test_observer_3 = larvae::RegisterTest("QueenObserver", "OnSetTriggerTypeDetection", []() {
        static_assert(queen::IsOnSetTrigger<queen::OnSet<Health>>);
        static_assert(!queen::IsOnSetTrigger<queen::OnAdd<Health>>);
        static_assert(!queen::IsOnSetTrigger<queen::OnRemove<Health>>);
        static_assert(!queen::IsOnSetTrigger<Health>);
    });

    auto test_observer_4 = larvae::RegisterTest("QueenObserver", "ObserverTriggerConcept", []() {
        static_assert(queen::ObserverTrigger<queen::OnAdd<Health>>);
        static_assert(queen::ObserverTrigger<queen::OnRemove<Health>>);
        static_assert(queen::ObserverTrigger<queen::OnSet<Health>>);
        static_assert(!queen::ObserverTrigger<Health>);
        static_assert(!queen::ObserverTrigger<int>);
    });

    auto test_observer_5 = larvae::RegisterTest("QueenObserver", "TriggerTypeExtraction", []() {
        larvae::AssertEqual(
            static_cast<int>(queen::GetTriggerType<queen::OnAdd<Health>>()),
            static_cast<int>(queen::TriggerType::Add)
        );
        larvae::AssertEqual(
            static_cast<int>(queen::GetTriggerType<queen::OnRemove<Health>>()),
            static_cast<int>(queen::TriggerType::Remove)
        );
        larvae::AssertEqual(
            static_cast<int>(queen::GetTriggerType<queen::OnSet<Health>>()),
            static_cast<int>(queen::TriggerType::Set)
        );
    });

    auto test_observer_6 = larvae::RegisterTest("QueenObserver", "ComponentIdExtraction", []() {
        queen::TypeId health_id = queen::TypeIdOf<Health>();
        queen::TypeId position_id = queen::TypeIdOf<Position>();

        larvae::AssertEqual(queen::GetTriggerComponentId<queen::OnAdd<Health>>(), health_id);
        larvae::AssertEqual(queen::GetTriggerComponentId<queen::OnRemove<Health>>(), health_id);
        larvae::AssertEqual(queen::GetTriggerComponentId<queen::OnSet<Position>>(), position_id);
    });

    // ─────────────────────────────────────────────────────────────
    // ObserverKey Tests
    // ─────────────────────────────────────────────────────────────

    auto test_observer_7 = larvae::RegisterTest("QueenObserver", "ObserverKeyCreation", []() {
        queen::ObserverKey key1 = queen::ObserverKey::Of<queen::OnAdd<Health>>();
        queen::ObserverKey key2 = queen::ObserverKey::Of<queen::OnRemove<Health>>();
        queen::ObserverKey key3 = queen::ObserverKey::Of<queen::OnAdd<Position>>();

        larvae::AssertEqual(static_cast<int>(key1.trigger), static_cast<int>(queen::TriggerType::Add));
        larvae::AssertEqual(key1.component_id, queen::TypeIdOf<Health>());

        larvae::AssertEqual(static_cast<int>(key2.trigger), static_cast<int>(queen::TriggerType::Remove));
        larvae::AssertEqual(key2.component_id, queen::TypeIdOf<Health>());

        larvae::AssertEqual(static_cast<int>(key3.trigger), static_cast<int>(queen::TriggerType::Add));
        larvae::AssertEqual(key3.component_id, queen::TypeIdOf<Position>());
    });

    auto test_observer_8 = larvae::RegisterTest("QueenObserver", "ObserverKeyEquality", []() {
        queen::ObserverKey key1 = queen::ObserverKey::Of<queen::OnAdd<Health>>();
        queen::ObserverKey key2 = queen::ObserverKey::Of<queen::OnAdd<Health>>();
        queen::ObserverKey key3 = queen::ObserverKey::Of<queen::OnRemove<Health>>();
        queen::ObserverKey key4 = queen::ObserverKey::Of<queen::OnAdd<Position>>();

        larvae::AssertTrue(key1 == key2);
        larvae::AssertFalse(key1 == key3); // Different trigger
        larvae::AssertFalse(key1 == key4); // Different component
    });

    auto test_observer_9 = larvae::RegisterTest("QueenObserver", "ObserverKeyHash", []() {
        queen::ObserverKeyHash hasher;
        queen::ObserverKey key1 = queen::ObserverKey::Of<queen::OnAdd<Health>>();
        queen::ObserverKey key2 = queen::ObserverKey::Of<queen::OnAdd<Health>>();
        queen::ObserverKey key3 = queen::ObserverKey::Of<queen::OnRemove<Health>>();

        // Same keys should have same hash
        larvae::AssertEqual(hasher(key1), hasher(key2));

        // Different keys should (likely) have different hashes
        larvae::AssertNotEqual(hasher(key1), hasher(key3));
    });

    // ─────────────────────────────────────────────────────────────
    // Observer Tests
    // ─────────────────────────────────────────────────────────────

    auto test_observer_10 = larvae::RegisterTest("QueenObserver", "ObserverConstruction", []() {
        comb::BuddyAllocator alloc{1024 * 1024};
        queen::ObserverId id{1};
        queen::Observer<comb::BuddyAllocator> observer{
            alloc, id, "TestObserver",
            queen::TriggerType::Add, queen::TypeIdOf<Health>()
        };

        // ID was passed as 1 to constructor
        larvae::AssertEqual(observer.Id().Value(), 1u);
        larvae::AssertTrue(std::strcmp(observer.Name(), "TestObserver") == 0);
        larvae::AssertEqual(static_cast<int>(observer.Trigger()), static_cast<int>(queen::TriggerType::Add));
        larvae::AssertEqual(observer.ComponentId(), queen::TypeIdOf<Health>());
        larvae::AssertTrue(observer.IsEnabled());
        larvae::AssertFalse(observer.HasCallback());
    });

    auto test_observer_11 = larvae::RegisterTest("QueenObserver", "ObserverEnableDisable", []() {
        comb::BuddyAllocator alloc{1024 * 1024};
        queen::ObserverId id{1};
        queen::Observer<comb::BuddyAllocator> observer{
            alloc, id, "TestObserver",
            queen::TriggerType::Add, queen::TypeIdOf<Health>()
        };

        larvae::AssertTrue(observer.IsEnabled());

        observer.SetEnabled(false);
        larvae::AssertFalse(observer.IsEnabled());

        observer.SetEnabled(true);
        larvae::AssertTrue(observer.IsEnabled());
    });

    auto test_observer_12 = larvae::RegisterTest("QueenObserver", "ObserverKey", []() {
        comb::BuddyAllocator alloc{1024 * 1024};
        queen::ObserverId id{1};
        queen::Observer<comb::BuddyAllocator> observer{
            alloc, id, "TestObserver",
            queen::TriggerType::Remove, queen::TypeIdOf<Position>()
        };

        queen::ObserverKey key = observer.Key();
        larvae::AssertEqual(static_cast<int>(key.trigger), static_cast<int>(queen::TriggerType::Remove));
        larvae::AssertEqual(key.component_id, queen::TypeIdOf<Position>());
    });

    // ─────────────────────────────────────────────────────────────
    // ObserverStorage Tests
    // ─────────────────────────────────────────────────────────────

    auto test_observer_13 = larvae::RegisterTest("QueenObserver", "StorageConstruction", []() {
        comb::BuddyAllocator alloc{1024 * 1024};
        queen::ObserverStorage<comb::BuddyAllocator> storage{alloc};

        larvae::AssertTrue(storage.IsEmpty());
        larvae::AssertEqual(storage.ObserverCount(), size_t{0});
    });

    // Test HashMap with ObserverKey and simple value
    auto test_observer_14_hashmap = larvae::RegisterTest("QueenObserver", "HashMapWithObserverKey", []() {
        comb::BuddyAllocator alloc{4 * 1024 * 1024};

        wax::HashMap<queen::ObserverKey, uint32_t, comb::BuddyAllocator, queen::ObserverKeyHash> map{alloc, 16};

        queen::ObserverKey key1 = queen::ObserverKey::Of<queen::OnAdd<Health>>();
        queen::ObserverKey key2 = queen::ObserverKey::Of<queen::OnRemove<Health>>();

        map.Insert(key1, 0);
        map.Insert(key2, 1);

        auto* val1 = map.Find(key1);
        auto* val2 = map.Find(key2);

        larvae::AssertNotNull(val1);
        larvae::AssertNotNull(val2);
        larvae::AssertEqual(*val1, 0u);
        larvae::AssertEqual(*val2, 1u);
    });

    // Test HashMap with Vector as value (like ObserverStorage uses)
    auto test_observer_14_hashmap_vec = larvae::RegisterTest("QueenObserver", "HashMapWithVectorValue", []() {
        comb::BuddyAllocator alloc{4 * 1024 * 1024};

        using VectorType = wax::Vector<uint32_t, comb::BuddyAllocator>;
        wax::HashMap<queen::ObserverKey, VectorType, comb::BuddyAllocator, queen::ObserverKeyHash> map{alloc, 16};

        queen::ObserverKey key1 = queen::ObserverKey::Of<queen::OnAdd<Health>>();

        VectorType indices{alloc};
        indices.PushBack(0);
        map.Insert(key1, std::move(indices));

        auto* vec = map.Find(key1);
        larvae::AssertNotNull(vec);
        larvae::AssertEqual(vec->Size(), size_t{1});
        larvae::AssertEqual((*vec)[0], 0u);
    });

    // Test Vector of Observers
    auto test_observer_14_vec_obs = larvae::RegisterTest("QueenObserver", "VectorOfObservers", []() {
        comb::BuddyAllocator alloc{4 * 1024 * 1024};

        wax::Vector<queen::Observer<comb::BuddyAllocator>, comb::BuddyAllocator> observers{alloc};

        queen::ObserverId id{0};
        observers.EmplaceBack(alloc, id, "TestObserver", queen::TriggerType::Add, queen::TypeIdOf<Health>());

        larvae::AssertEqual(observers.Size(), size_t{1});
        larvae::AssertTrue(std::strcmp(observers[0].Name(), "TestObserver") == 0);
    });

    // Test ObserverStorage without World
    auto test_observer_14_storage_no_world = larvae::RegisterTest("QueenObserver", "StorageWithoutWorld", []() {
        comb::BuddyAllocator alloc{4 * 1024 * 1024};
        queen::ObserverStorage<comb::BuddyAllocator> storage{alloc};

        // Manually add observer to test internal structures
        larvae::AssertTrue(storage.IsEmpty());
        larvae::AssertEqual(storage.ObserverCount(), size_t{0});
    });

    // Test creating World only
    auto test_observer_14_world_only = larvae::RegisterTest("QueenObserver", "WorldOnly", []() {
        queen::World world{};
        larvae::AssertTrue(true);
    });

    // Test World + Storage (no Register)
    auto test_observer_14_world_storage = larvae::RegisterTest("QueenObserver", "WorldAndStorage", []() {
        comb::BuddyAllocator alloc{4 * 1024 * 1024};
        queen::ObserverStorage<comb::BuddyAllocator> storage{alloc};
        queen::World world{};

        larvae::AssertTrue(storage.IsEmpty());
    });

    // Test manual Register steps
    auto test_observer_14_manual = larvae::RegisterTest("QueenObserver", "ManualRegisterSteps", []() {
        comb::BuddyAllocator alloc{4 * 1024 * 1024};

        // Create storage
        wax::Vector<queen::Observer<comb::BuddyAllocator>, comb::BuddyAllocator> observers{alloc};
        wax::HashMap<queen::ObserverKey, wax::Vector<uint32_t, comb::BuddyAllocator>,
                     comb::BuddyAllocator, queen::ObserverKeyHash> lookup{alloc, 32};

        // Step 1: Create observer ID
        queen::ObserverId id{static_cast<uint32_t>(observers.Size())};
        larvae::AssertEqual(id.Value(), 0u);

        // Step 2: Get trigger info
        queen::TriggerType trigger = queen::GetTriggerType<queen::OnAdd<Health>>();
        queen::TypeId component_id = queen::GetTriggerComponentId<queen::OnAdd<Health>>();
        larvae::AssertEqual(static_cast<int>(trigger), static_cast<int>(queen::TriggerType::Add));

        // Step 3: EmplaceBack observer
        observers.EmplaceBack(alloc, id, "TestObserver", trigger, component_id);
        larvae::AssertEqual(observers.Size(), size_t{1});

        // Step 4: Create key and add to lookup
        queen::ObserverKey key = queen::ObserverKey::Of<queen::OnAdd<Health>>();
        wax::Vector<uint32_t, comb::BuddyAllocator> indices{alloc};
        indices.PushBack(id.Value());
        lookup.Insert(key, std::move(indices));

        // Verify
        auto* found = lookup.Find(key);
        larvae::AssertNotNull(found);
        larvae::AssertEqual(found->Size(), size_t{1});
    });

    // Test manual Register steps with World present but using ObserverStorage::Register
    auto test_observer_14_manual_with_world = larvae::RegisterTest("QueenObserver", "ManualRegisterStepsWithWorld", []() {
        comb::BuddyAllocator alloc{4 * 1024 * 1024};
        queen::World world{};  // Create World first

        // Manually do what ObserverStorage does internally
        wax::Vector<queen::Observer<comb::BuddyAllocator>, comb::BuddyAllocator> observers{alloc};
        wax::HashMap<queen::ObserverKey, wax::Vector<uint32_t, comb::BuddyAllocator>,
                     comb::BuddyAllocator, queen::ObserverKeyHash> lookup{alloc, 32};

        // Step 1-4: Same as before
        queen::ObserverId id{static_cast<uint32_t>(observers.Size())};
        queen::TriggerType trigger = queen::GetTriggerType<queen::OnAdd<Health>>();
        queen::TypeId component_id = queen::GetTriggerComponentId<queen::OnAdd<Health>>();
        observers.EmplaceBack(alloc, id, "TestObserver", trigger, component_id);

        queen::ObserverKey key = queen::ObserverKey::Of<queen::OnAdd<Health>>();
        wax::Vector<uint32_t, comb::BuddyAllocator> indices{alloc};
        indices.PushBack(id.Value());
        lookup.Insert(key, std::move(indices));

        larvae::AssertEqual(observers.Size(), size_t{1});

        // Access the observer
        auto* obs = &observers[0];
        larvae::AssertTrue(std::strcmp(obs->Name(), "TestObserver") == 0);
    });

    // Test using actual ObserverStorage::Register with World
    auto test_observer_14_actual_register = larvae::RegisterTest("QueenObserver", "ActualRegisterWithWorld", []() {
        comb::BuddyAllocator alloc{4 * 1024 * 1024};
        queen::World world{};  // Create World first
        queen::ObserverStorage<comb::BuddyAllocator> storage{alloc};

        // Just call Register, don't do anything with the builder
        storage.Register<queen::OnAdd<Health>>(world, "HealthAdded");

        larvae::AssertEqual(storage.ObserverCount(), size_t{1});
    });

    auto test_observer_14 = larvae::RegisterTest("QueenObserver", "StorageRegisterObserverSimple", []() {
        // Use larger allocator to avoid out of memory issues
        comb::BuddyAllocator alloc{4 * 1024 * 1024};

        // Create World FIRST (this order matters!)
        queen::World world{};
        queen::ObserverStorage<comb::BuddyAllocator> storage{alloc};

        // Register without callback first
        auto builder = storage.Register<queen::OnAdd<Health>>(world, "HealthAdded");
        queen::ObserverId id = builder.Id();

        larvae::AssertTrue(id.IsValid());
        larvae::AssertFalse(storage.IsEmpty());
        larvae::AssertEqual(storage.ObserverCount(), size_t{1});

        auto* obs = storage.GetObserver(id);
        larvae::AssertNotNull(obs);
        larvae::AssertTrue(std::strcmp(obs->Name(), "HealthAdded") == 0);
    });

    // Test allocator works after World creation
    auto test_observer_14b_alloc = larvae::RegisterTest("QueenObserver", "AllocatorAfterWorld", []() {
        comb::BuddyAllocator alloc{4 * 1024 * 1024};
        queen::World world{};

        // Try to allocate from the allocator after World is created
        void* ptr = alloc.Allocate(64, 8);
        larvae::AssertNotNull(ptr);
        alloc.Deallocate(ptr);
    });

    // Test allocator via storage after World
    auto test_observer_14b_storage_alloc = larvae::RegisterTest("QueenObserver", "StorageAllocAfterWorld", []() {
        comb::BuddyAllocator alloc{4 * 1024 * 1024};
        queen::World world{};
        queen::ObserverStorage<comb::BuddyAllocator> storage{alloc};

        // Just call Register but don't use EachEntity yet
        auto builder = storage.Register<queen::OnAdd<Health>>(world, "HealthAdded");

        // Now try to allocate from the same allocator the builder would use
        void* ptr = alloc.Allocate(64, 8);
        larvae::AssertNotNull(ptr);
        alloc.Deallocate(ptr);

        // Check the ID works
        queen::ObserverId id = builder.Id();
        larvae::AssertTrue(id.IsValid());
    });

    // Simple callback without captures
    auto test_observer_14b_simple = larvae::RegisterTest("QueenObserver", "StorageRegisterWithEmptyCallback", []() {
        comb::BuddyAllocator alloc{4 * 1024 * 1024};
        queen::World world{};  // World must be created before storage
        queen::ObserverStorage<comb::BuddyAllocator> storage{alloc};

        // Simplest possible callback - no captures
        queen::ObserverId id = storage.Register<queen::OnAdd<Health>>(world, "HealthAdded")
            .EachEntity([](queen::Entity e) {
                (void)e;
            });

        larvae::AssertTrue(id.IsValid());
        larvae::AssertEqual(storage.ObserverCount(), size_t{1});

        auto* obs = storage.GetObserver(id);
        larvae::AssertNotNull(obs);
        larvae::AssertTrue(obs->HasCallback());
    });

    auto test_observer_14b = larvae::RegisterTest("QueenObserver", "StorageRegisterObserverWithCallback", []() {
        comb::BuddyAllocator alloc{4 * 1024 * 1024};
        queen::World world{};  // World must be created before storage
        queen::ObserverStorage<comb::BuddyAllocator> storage{alloc};

        int call_count = 0;
        queen::ObserverId id = storage.Register<queen::OnAdd<Health>>(world, "HealthAdded")
            .EachEntity([&call_count](queen::Entity e) {
                (void)e;
                ++call_count;
            });

        larvae::AssertTrue(id.IsValid());
        larvae::AssertEqual(storage.ObserverCount(), size_t{1});

        auto* obs = storage.GetObserver(id);
        larvae::AssertNotNull(obs);
        larvae::AssertTrue(obs->HasCallback());
    });

    auto test_observer_15 = larvae::RegisterTest("QueenObserver", "StorageTriggerObserver", []() {
        comb::BuddyAllocator alloc{1024 * 1024};
        queen::World world{};  // World must be created before storage
        queen::ObserverStorage<comb::BuddyAllocator> storage{alloc};

        int call_count = 0;
        queen::Entity received_entity;

        storage.Register<queen::OnAdd<Health>>(world, "HealthAdded")
            .EachEntity([&call_count, &received_entity](queen::Entity e) {
                ++call_count;
                received_entity = e;
            });

        // Trigger the observer
        queen::Entity test_entity{42, 1};
        Health hp{100.0f, 100.0f};
        storage.Trigger(queen::TriggerType::Add, queen::TypeIdOf<Health>(),
                       world, test_entity, &hp);

        larvae::AssertEqual(call_count, 1);
        larvae::AssertEqual(received_entity.Index(), 42u);
    });

    auto test_observer_16 = larvae::RegisterTest("QueenObserver", "StorageTriggerWithComponent", []() {
        comb::BuddyAllocator alloc{1024 * 1024};
        queen::World world{};  // World must be created before storage
        queen::ObserverStorage<comb::BuddyAllocator> storage{alloc};

        float received_health = 0.0f;

        storage.Register<queen::OnAdd<Health>>(world, "HealthAdded")
            .Each([&received_health](queen::Entity e, const Health& hp) {
                (void)e;
                received_health = hp.value;
            });

        // Trigger the observer
        queen::Entity test_entity{1, 1};
        Health hp{75.5f, 100.0f};
        storage.Trigger(queen::TriggerType::Add, queen::TypeIdOf<Health>(),
                       world, test_entity, &hp);

        larvae::AssertEqual(received_health, 75.5f);
    });

    auto test_observer_17 = larvae::RegisterTest("QueenObserver", "StorageMultipleObserversSameKey", []() {
        comb::BuddyAllocator alloc{1024 * 1024};
        queen::World world{};  // World must be created before storage
        queen::ObserverStorage<comb::BuddyAllocator> storage{alloc};

        int count1 = 0;
        int count2 = 0;

        storage.Register<queen::OnAdd<Health>>(world, "Observer1")
            .EachEntity([&count1](queen::Entity) { ++count1; });

        storage.Register<queen::OnAdd<Health>>(world, "Observer2")
            .EachEntity([&count2](queen::Entity) { ++count2; });

        larvae::AssertEqual(storage.ObserverCount(), size_t{2});

        // Trigger - both observers should be called
        queen::Entity test_entity{1, 1};
        Health hp{100.0f, 100.0f};
        storage.Trigger(queen::TriggerType::Add, queen::TypeIdOf<Health>(),
                       world, test_entity, &hp);

        larvae::AssertEqual(count1, 1);
        larvae::AssertEqual(count2, 1);
    });

    auto test_observer_18 = larvae::RegisterTest("QueenObserver", "StorageNoTriggerForDifferentKey", []() {
        comb::BuddyAllocator alloc{1024 * 1024};
        queen::World world{};  // World must be created before storage
        queen::ObserverStorage<comb::BuddyAllocator> storage{alloc};

        int health_add_count = 0;
        int position_add_count = 0;

        storage.Register<queen::OnAdd<Health>>(world, "HealthAdded")
            .EachEntity([&health_add_count](queen::Entity) { ++health_add_count; });

        storage.Register<queen::OnAdd<Position>>(world, "PositionAdded")
            .EachEntity([&position_add_count](queen::Entity) { ++position_add_count; });

        // Trigger only Health add
        queen::Entity test_entity{1, 1};
        Health hp{100.0f, 100.0f};
        storage.Trigger(queen::TriggerType::Add, queen::TypeIdOf<Health>(),
                       world, test_entity, &hp);

        larvae::AssertEqual(health_add_count, 1);
        larvae::AssertEqual(position_add_count, 0);
    });

    auto test_observer_19 = larvae::RegisterTest("QueenObserver", "StorageDisabledObserverNotTriggered", []() {
        comb::BuddyAllocator alloc{1024 * 1024};
        queen::World world{};  // World must be created before storage
        queen::ObserverStorage<comb::BuddyAllocator> storage{alloc};

        int call_count = 0;

        queen::ObserverId id = storage.Register<queen::OnAdd<Health>>(world, "HealthAdded")
            .EachEntity([&call_count](queen::Entity) { ++call_count; });

        // Disable the observer
        storage.SetEnabled(id, false);

        // Trigger - should not call disabled observer
        queen::Entity test_entity{1, 1};
        Health hp{100.0f, 100.0f};
        storage.Trigger(queen::TriggerType::Add, queen::TypeIdOf<Health>(),
                       world, test_entity, &hp);

        larvae::AssertEqual(call_count, 0);

        // Re-enable and trigger again
        storage.SetEnabled(id, true);
        storage.Trigger(queen::TriggerType::Add, queen::TypeIdOf<Health>(),
                       world, test_entity, &hp);

        larvae::AssertEqual(call_count, 1);
    });

    auto test_observer_20 = larvae::RegisterTest("QueenObserver", "StorageHasObservers", []() {
        comb::BuddyAllocator alloc{1024 * 1024};
        queen::World world{};  // World must be created before storage
        queen::ObserverStorage<comb::BuddyAllocator> storage{alloc};

        larvae::AssertFalse(storage.HasObservers<queen::OnAdd<Health>>());

        storage.Register<queen::OnAdd<Health>>(world, "HealthAdded")
            .EachEntity([](queen::Entity) {});

        larvae::AssertTrue(storage.HasObservers<queen::OnAdd<Health>>());
        larvae::AssertFalse(storage.HasObservers<queen::OnRemove<Health>>());
        larvae::AssertFalse(storage.HasObservers<queen::OnAdd<Position>>());
    });

    auto test_observer_21 = larvae::RegisterTest("QueenObserver", "StorageGetByName", []() {
        comb::BuddyAllocator alloc{1024 * 1024};
        queen::World world{};  // World must be created before storage
        queen::ObserverStorage<comb::BuddyAllocator> storage{alloc};

        storage.Register<queen::OnAdd<Health>>(world, "HealthAdded")
            .EachEntity([](queen::Entity) {});

        storage.Register<queen::OnRemove<Health>>(world, "HealthRemoved")
            .EachEntity([](queen::Entity) {});

        auto* obs1 = storage.GetObserverByName("HealthAdded");
        auto* obs2 = storage.GetObserverByName("HealthRemoved");
        auto* obs3 = storage.GetObserverByName("NonExistent");

        larvae::AssertNotNull(obs1);
        larvae::AssertNotNull(obs2);
        larvae::AssertNull(obs3);

        larvae::AssertTrue(std::strcmp(obs1->Name(), "HealthAdded") == 0);
        larvae::AssertTrue(std::strcmp(obs2->Name(), "HealthRemoved") == 0);
    });

    auto test_observer_22 = larvae::RegisterTest("QueenObserver", "OnRemoveTrigger", []() {
        comb::BuddyAllocator alloc{1024 * 1024};
        queen::World world{};  // World must be created before storage
        queen::ObserverStorage<comb::BuddyAllocator> storage{alloc};

        int call_count = 0;

        storage.Register<queen::OnRemove<Health>>(world, "HealthRemoved")
            .EachEntity([&call_count](queen::Entity) { ++call_count; });

        // Trigger OnAdd - should not call OnRemove observer
        queen::Entity test_entity{1, 1};
        Health hp{100.0f, 100.0f};
        storage.Trigger(queen::TriggerType::Add, queen::TypeIdOf<Health>(),
                       world, test_entity, &hp);
        larvae::AssertEqual(call_count, 0);

        // Trigger OnRemove - should call the observer
        storage.Trigger(queen::TriggerType::Remove, queen::TypeIdOf<Health>(),
                       world, test_entity, &hp);
        larvae::AssertEqual(call_count, 1);
    });

    auto test_observer_23 = larvae::RegisterTest("QueenObserver", "OnSetTrigger", []() {
        comb::BuddyAllocator alloc{1024 * 1024};
        queen::World world{};  // World must be created before storage
        queen::ObserverStorage<comb::BuddyAllocator> storage{alloc};

        float received_value = 0.0f;

        storage.Register<queen::OnSet<Health>>(world, "HealthChanged")
            .Each([&received_value](queen::Entity, const Health& hp) {
                received_value = hp.value;
            });

        // Trigger OnSet
        queen::Entity test_entity{1, 1};
        Health hp{50.0f, 100.0f};
        storage.Trigger(queen::TriggerType::Set, queen::TypeIdOf<Health>(),
                       world, test_entity, &hp);

        larvae::AssertEqual(received_value, 50.0f);
    });

    auto test_observer_24 = larvae::RegisterTest("QueenObserver", "TypeSafeTrigger", []() {
        comb::BuddyAllocator alloc{1024 * 1024};
        queen::World world{};  // World must be created before storage
        queen::ObserverStorage<comb::BuddyAllocator> storage{alloc};

        int call_count = 0;

        storage.Register<queen::OnAdd<Health>>(world, "HealthAdded")
            .EachEntity([&call_count](queen::Entity) { ++call_count; });

        // Use type-safe trigger
        queen::Entity test_entity{1, 1};
        Health hp{100.0f, 100.0f};
        storage.Trigger<queen::OnAdd<Health>>(world, test_entity, &hp);

        larvae::AssertEqual(call_count, 1);
    });
}
