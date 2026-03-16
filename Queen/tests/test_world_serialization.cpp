#include <queen/reflect/component_registry.h>
#include <queen/reflect/reflectable.h>
#include <queen/reflect/world_deserializer.h>
#include <queen/reflect/world_serializer.h>
#include <queen/world/world.h>

#include <larvae/larvae.h>

#include <cstring>

namespace
{
    // ============================================================
    // Test types
    // ============================================================

    struct Pos
    {
        float x = 0.f;
        float y = 0.f;
        float z = 0.f;

        static void Reflect(queen::ComponentReflector<>& r)
        {
            r.Field("x", &Pos::x);
            r.Field("y", &Pos::y);
            r.Field("z", &Pos::z);
        }
    };

    struct Vel
    {
        float dx = 0.f;
        float dy = 0.f;
        float dz = 0.f;

        static void Reflect(queen::ComponentReflector<>& r)
        {
            r.Field("dx", &Vel::dx);
            r.Field("dy", &Vel::dy);
            r.Field("dz", &Vel::dz);
        }
    };

    struct Targeting
    {
        queen::Entity target;
        int32_t priority = 0;

        static void Reflect(queen::ComponentReflector<>& r)
        {
            r.Field("target", &Targeting::target);
            r.Field("priority", &Targeting::priority);
        }
    };

    struct Health
    {
        int32_t current = 100;
        int32_t max = 100;

        static void Reflect(queen::ComponentReflector<>& r)
        {
            r.Field("current", &Health::current);
            r.Field("max", &Health::max);
        }
    };

    // ============================================================
    // Helpers
    // ============================================================

    bool HasEntityWithPos(queen::World& world, float ex, float ey, float ez)
    {
        bool found = false;
        world.ForEachArchetype([&](queen::Archetype<queen::ComponentAllocator>& arch) {
            if (!arch.template HasComponent<Pos>())
                return;
            for (uint32_t row = 0; row < arch.EntityCount(); ++row)
            {
                auto* p = arch.template GetComponent<Pos>(row);
                if (p && p->x == ex && p->y == ey && p->z == ez)
                {
                    found = true;
                }
            }
        });
        return found;
    }

    queen::Entity FindEntityWithPos(queen::World& world, float ex, float ey, float ez)
    {
        queen::Entity result;
        world.ForEachArchetype([&](queen::Archetype<queen::ComponentAllocator>& arch) {
            if (!arch.template HasComponent<Pos>())
                return;
            for (uint32_t row = 0; row < arch.EntityCount(); ++row)
            {
                auto* p = arch.template GetComponent<Pos>(row);
                if (p && p->x == ex && p->y == ey && p->z == ez)
                {
                    result = arch.GetEntity(row);
                }
            }
        });
        return result;
    }

    // ============================================================
    // Serialize tests
    // ============================================================

    auto test_serialize_empty = larvae::RegisterTest("QueenWorldSerialization", "SerializeEmpty", []() {
        queen::ComponentRegistry<32> registry;
        registry.Register<Pos>();

        queen::World world;

        queen::WorldSerializer<4096> serializer;
        auto result = serializer.Serialize(world, registry);

        larvae::AssertTrue(result.m_success);
        larvae::AssertEqual(result.m_entitiesWritten, size_t{0});
        larvae::AssertTrue(std::strstr(serializer.CStr(), "\"version\":1") != nullptr);
        larvae::AssertTrue(std::strstr(serializer.CStr(), "\"entities\":[]") != nullptr);
    });

    auto test_serialize_counts = larvae::RegisterTest("QueenWorldSerialization", "SerializeCounts", []() {
        queen::ComponentRegistry<32> registry;
        registry.Register<Pos>();
        registry.Register<Vel>();

        queen::World world;
        static_cast<void>(world.Spawn(Pos{1.f, 0.f, 0.f}));
        static_cast<void>(world.Spawn(Pos{2.f, 0.f, 0.f}, Vel{0.1f, 0.f, 0.f}));

        queen::WorldSerializer<8192> serializer;
        auto result = serializer.Serialize(world, registry);

        larvae::AssertTrue(result.m_success);
        larvae::AssertEqual(result.m_entitiesWritten, size_t{2});
        larvae::AssertEqual(result.m_componentsWritten, size_t{3}); // Pos + Pos + Vel
    });

    auto test_serialize_skips_unregistered =
        larvae::RegisterTest("QueenWorldSerialization", "SerializeSkipsUnregistered", []() {
            queen::ComponentRegistry<32> registry;
            registry.Register<Pos>();
            // Vel is NOT registered

            queen::World world;
            static_cast<void>(world.Spawn(Pos{1.f, 0.f, 0.f}, Vel{0.1f, 0.f, 0.f}));

            queen::WorldSerializer<4096> serializer;
            auto result = serializer.Serialize(world, registry);

            larvae::AssertTrue(result.m_success);
            larvae::AssertEqual(result.m_entitiesWritten, size_t{1});
            larvae::AssertEqual(result.m_componentsWritten, size_t{1}); // Only Pos
        });

    auto test_serialize_parent_field = larvae::RegisterTest("QueenWorldSerialization", "SerializeParentField", []() {
        queen::ComponentRegistry<32> registry;
        registry.Register<Pos>();

        queen::World world;
        queen::Entity parent = world.Spawn(Pos{1.f, 0.f, 0.f});
        queen::Entity child = world.Spawn(Pos{2.f, 0.f, 0.f});
        world.SetParent(child, parent);

        queen::WorldSerializer<8192> serializer;
        auto result = serializer.Serialize(world, registry);

        larvae::AssertTrue(result.m_success);
        // Parent field should appear in the JSON
        larvae::AssertTrue(std::strstr(serializer.CStr(), "\"parent\":") != nullptr);
    });

    // ============================================================
    // Roundtrip tests
    // ============================================================

    auto test_roundtrip_empty = larvae::RegisterTest("QueenWorldSerialization", "RoundtripEmpty", []() {
        queen::ComponentRegistry<32> registry;
        registry.Register<Pos>();

        queen::World src;

        queen::WorldSerializer<4096> serializer;
        serializer.Serialize(src, registry);

        queen::World dst;
        auto result = queen::WorldDeserializer::Deserialize(dst, registry, serializer.CStr());

        larvae::AssertTrue(result.m_success);
        larvae::AssertEqual(result.m_entitiesLoaded, size_t{0});
        larvae::AssertEqual(dst.EntityCount(), size_t{0});
    });

    auto test_roundtrip_single = larvae::RegisterTest("QueenWorldSerialization", "RoundtripSingle", []() {
        queen::ComponentRegistry<32> registry;
        registry.Register<Pos>();

        queen::World src;
        static_cast<void>(src.Spawn(Pos{1.5f, -2.5f, 3.0f}));

        queen::WorldSerializer<4096> serializer;
        larvae::AssertTrue(serializer.Serialize(src, registry).m_success);

        queen::World dst;
        auto result = queen::WorldDeserializer::Deserialize(dst, registry, serializer.CStr());

        larvae::AssertTrue(result.m_success);
        larvae::AssertEqual(dst.EntityCount(), size_t{1});
        larvae::AssertTrue(HasEntityWithPos(dst, 1.5f, -2.5f, 3.0f));
    });

    auto test_roundtrip_multi_entity = larvae::RegisterTest("QueenWorldSerialization", "RoundtripMultiEntity", []() {
        queen::ComponentRegistry<32> registry;
        registry.Register<Pos>();

        queen::World src;
        static_cast<void>(src.Spawn(Pos{1.f, 0.f, 0.f}));
        static_cast<void>(src.Spawn(Pos{2.f, 0.f, 0.f}));
        static_cast<void>(src.Spawn(Pos{3.f, 0.f, 0.f}));

        queen::WorldSerializer<8192> serializer;
        larvae::AssertTrue(serializer.Serialize(src, registry).m_success);

        queen::World dst;
        auto result = queen::WorldDeserializer::Deserialize(dst, registry, serializer.CStr());

        larvae::AssertTrue(result.m_success);
        larvae::AssertEqual(dst.EntityCount(), size_t{3});
        larvae::AssertTrue(HasEntityWithPos(dst, 1.f, 0.f, 0.f));
        larvae::AssertTrue(HasEntityWithPos(dst, 2.f, 0.f, 0.f));
        larvae::AssertTrue(HasEntityWithPos(dst, 3.f, 0.f, 0.f));
    });

    auto test_roundtrip_multi_archetype =
        larvae::RegisterTest("QueenWorldSerialization", "RoundtripMultiArchetype", []() {
            queen::ComponentRegistry<32> registry;
            registry.Register<Pos>();
            registry.Register<Vel>();

            queen::World src;
            static_cast<void>(src.Spawn(Pos{1.f, 0.f, 0.f}));                      // archetype: [Pos]
            static_cast<void>(src.Spawn(Pos{2.f, 0.f, 0.f}, Vel{0.5f, 0.f, 0.f})); // archetype: [Pos, Vel]

            queen::WorldSerializer<8192> serializer;
            larvae::AssertTrue(serializer.Serialize(src, registry).m_success);

            queen::World dst;
            auto result = queen::WorldDeserializer::Deserialize(dst, registry, serializer.CStr());

            larvae::AssertTrue(result.m_success);
            larvae::AssertEqual(dst.EntityCount(), size_t{2});
            larvae::AssertEqual(result.m_componentsLoaded, size_t{3});

            // Check entity with only Pos
            larvae::AssertTrue(HasEntityWithPos(dst, 1.f, 0.f, 0.f));

            // Check entity with Pos + Vel
            queen::Entity e = FindEntityWithPos(dst, 2.f, 0.f, 0.f);
            larvae::AssertTrue(dst.IsAlive(e));
            auto* vel = dst.Get<Vel>(e);
            larvae::AssertNotNull(vel);
            larvae::AssertEqual(vel->dx, 0.5f);
        });

    auto test_roundtrip_multi_component =
        larvae::RegisterTest("QueenWorldSerialization", "RoundtripMultiComponent", []() {
            queen::ComponentRegistry<32> registry;
            registry.Register<Pos>();
            registry.Register<Health>();

            queen::World src;
            static_cast<void>(src.Spawn(Pos{5.f, 10.f, 15.f}, Health{75, 100}));

            queen::WorldSerializer<4096> serializer;
            larvae::AssertTrue(serializer.Serialize(src, registry).m_success);

            queen::World dst;
            queen::WorldDeserializer::Deserialize(dst, registry, serializer.CStr());

            queen::Entity e = FindEntityWithPos(dst, 5.f, 10.f, 15.f);
            larvae::AssertTrue(dst.IsAlive(e));
            auto* hp = dst.Get<Health>(e);
            larvae::AssertNotNull(hp);
            larvae::AssertEqual(hp->current, int32_t{75});
            larvae::AssertEqual(hp->max, int32_t{100});
        });

    // ============================================================
    // Entity remapping tests
    // ============================================================

    auto test_entity_remapping = larvae::RegisterTest("QueenWorldSerialization", "EntityRemapping", []() {
        queen::ComponentRegistry<32> registry;
        registry.Register<Pos>();
        registry.Register<Targeting>();

        queen::World src;
        queen::Entity target = src.Spawn(Pos{10.f, 20.f, 30.f});
        static_cast<void>(src.Spawn(Pos{0.f, 0.f, 0.f}, Targeting{target, 5}));

        queen::WorldSerializer<8192> serializer;
        larvae::AssertTrue(serializer.Serialize(src, registry).m_success);

        queen::World dst;
        auto result = queen::WorldDeserializer::Deserialize(dst, registry, serializer.CStr());

        larvae::AssertTrue(result.m_success);
        larvae::AssertEqual(dst.EntityCount(), size_t{2});

        // Find the targeting entity and verify Entity reference was remapped
        bool found = false;
        dst.ForEachArchetype([&](queen::Archetype<queen::ComponentAllocator>& arch) {
            if (!arch.template HasComponent<Targeting>())
                return;
            for (uint32_t row = 0; row < arch.EntityCount(); ++row)
            {
                auto* t = arch.template GetComponent<Targeting>(row);
                if (t)
                {
                    // Target reference should point to a live entity in dst
                    larvae::AssertTrue(dst.IsAlive(t->target));
                    // And that entity should have Pos{10, 20, 30}
                    auto* pos = dst.Get<Pos>(t->target);
                    larvae::AssertNotNull(pos);
                    larvae::AssertEqual(pos->x, 10.f);
                    larvae::AssertEqual(pos->y, 20.f);
                    larvae::AssertEqual(pos->z, 30.f);
                    larvae::AssertEqual(t->priority, int32_t{5});
                    found = true;
                }
            }
        });
        larvae::AssertTrue(found);
    });

    // ============================================================
    // Hierarchy tests
    // ============================================================

    auto test_hierarchy_preserved = larvae::RegisterTest("QueenWorldSerialization", "HierarchyPreserved", []() {
        queen::ComponentRegistry<32> registry;
        registry.Register<Pos>();

        queen::World src;
        queen::Entity parent = src.Spawn(Pos{1.f, 0.f, 0.f});
        queen::Entity child = src.Spawn(Pos{2.f, 0.f, 0.f});
        src.SetParent(child, parent);

        queen::WorldSerializer<8192> serializer;
        serializer.Serialize(src, registry);

        queen::World dst;
        auto result = queen::WorldDeserializer::Deserialize(dst, registry, serializer.CStr());

        larvae::AssertTrue(result.m_success);
        larvae::AssertEqual(dst.EntityCount(), size_t{2});

        queen::Entity dst_child = FindEntityWithPos(dst, 2.f, 0.f, 0.f);
        queen::Entity dst_parent = FindEntityWithPos(dst, 1.f, 0.f, 0.f);

        larvae::AssertTrue(dst.IsAlive(dst_child));
        larvae::AssertTrue(dst.IsAlive(dst_parent));
        larvae::AssertTrue(dst.HasParent(dst_child));
        larvae::AssertTrue(dst.GetParent(dst_child) == dst_parent);
        larvae::AssertFalse(dst.HasParent(dst_parent));
    });

    auto test_hierarchy_deep = larvae::RegisterTest("QueenWorldSerialization", "HierarchyDeep", []() {
        queen::ComponentRegistry<32> registry;
        registry.Register<Pos>();

        queen::World src;
        queen::Entity root = src.Spawn(Pos{1.f, 0.f, 0.f});
        queen::Entity mid = src.Spawn(Pos{2.f, 0.f, 0.f});
        queen::Entity leaf = src.Spawn(Pos{3.f, 0.f, 0.f});
        src.SetParent(mid, root);
        src.SetParent(leaf, mid);

        queen::WorldSerializer<8192> serializer;
        serializer.Serialize(src, registry);

        queen::World dst;
        queen::WorldDeserializer::Deserialize(dst, registry, serializer.CStr());

        queen::Entity d_root = FindEntityWithPos(dst, 1.f, 0.f, 0.f);
        queen::Entity d_mid = FindEntityWithPos(dst, 2.f, 0.f, 0.f);
        queen::Entity d_leaf = FindEntityWithPos(dst, 3.f, 0.f, 0.f);

        larvae::AssertFalse(dst.HasParent(d_root));
        larvae::AssertTrue(dst.GetParent(d_mid) == d_root);
        larvae::AssertTrue(dst.GetParent(d_leaf) == d_mid);
    });

    // ============================================================
    // Forward-compatibility tests
    // ============================================================

    auto test_unknown_component_skipped =
        larvae::RegisterTest("QueenWorldSerialization", "UnknownComponentSkipped", []() {
            // Serialize with full registry
            queen::ComponentRegistry<32> full_registry;
            full_registry.Register<Pos>();
            full_registry.Register<Vel>();

            queen::World src;
            static_cast<void>(src.Spawn(Pos{1.f, 2.f, 3.f}, Vel{0.1f, 0.2f, 0.3f}));

            queen::WorldSerializer<8192> serializer;
            larvae::AssertTrue(serializer.Serialize(src, full_registry).m_success);

            // Deserialize with partial registry (no Vel)
            queen::ComponentRegistry<32> partial_registry;
            partial_registry.Register<Pos>();

            queen::World dst;
            auto result = queen::WorldDeserializer::Deserialize(dst, partial_registry, serializer.CStr());

            larvae::AssertTrue(result.m_success);
            larvae::AssertEqual(result.m_entitiesLoaded, size_t{1});
            larvae::AssertEqual(result.m_componentsLoaded, size_t{1});
            larvae::AssertEqual(result.m_componentsSkipped, size_t{1});
            larvae::AssertTrue(HasEntityWithPos(dst, 1.f, 2.f, 3.f));
        });

    auto test_additive_load = larvae::RegisterTest("QueenWorldSerialization", "AdditiveLoad", []() {
        queen::ComponentRegistry<32> registry;
        registry.Register<Pos>();

        // Pre-existing entity in destination
        queen::World dst;
        static_cast<void>(dst.Spawn(Pos{99.f, 99.f, 99.f}));
        larvae::AssertEqual(dst.EntityCount(), size_t{1});

        // Serialize source
        queen::World src;
        static_cast<void>(src.Spawn(Pos{1.f, 2.f, 3.f}));

        queen::WorldSerializer<4096> serializer;
        larvae::AssertTrue(serializer.Serialize(src, registry).m_success);

        // Load additively
        auto result = queen::WorldDeserializer::Deserialize(dst, registry, serializer.CStr());

        larvae::AssertTrue(result.m_success);
        larvae::AssertEqual(result.m_entitiesLoaded, size_t{1});
        larvae::AssertEqual(dst.EntityCount(), size_t{2});
        larvae::AssertTrue(HasEntityWithPos(dst, 99.f, 99.f, 99.f)); // original preserved
        larvae::AssertTrue(HasEntityWithPos(dst, 1.f, 2.f, 3.f));    // loaded
    });

    // ============================================================
    // Edge cases
    // ============================================================

    auto test_entity_no_components = larvae::RegisterTest("QueenWorldSerialization", "EntityNoComponents", []() {
        queen::ComponentRegistry<32> registry;
        // Register nothing — entity has no serializable components

        queen::World src;
        static_cast<void>(src.Spawn(Pos{1.f, 0.f, 0.f})); // Pos is not registered

        queen::WorldSerializer<4096> serializer;
        auto result = serializer.Serialize(src, registry);

        larvae::AssertTrue(result.m_success);
        larvae::AssertEqual(result.m_entitiesWritten, size_t{1});
        larvae::AssertEqual(result.m_componentsWritten, size_t{0});
    });

    auto test_empty_archetype_entity = larvae::RegisterTest("QueenWorldSerialization", "EmptyArchetypeEntity", []() {
        queen::ComponentRegistry<32> registry;
        registry.Register<Pos>();

        queen::World src;
        const queen::Entity entity = src.Spawn().Build();
        larvae::AssertTrue(src.IsAlive(entity));

        queen::WorldSerializer<4096> serializer;
        const auto serializeResult = serializer.Serialize(src, registry);

        larvae::AssertTrue(serializeResult.m_success);
        larvae::AssertEqual(serializeResult.m_entitiesWritten, size_t{1});
        larvae::AssertEqual(serializeResult.m_componentsWritten, size_t{0});
        larvae::AssertTrue(std::strstr(serializer.CStr(), "\"components\":{}") != nullptr);

        queen::World dst;
        const auto deserializeResult = queen::WorldDeserializer::Deserialize(dst, registry, serializer.CStr());

        larvae::AssertTrue(deserializeResult.m_success);
        larvae::AssertEqual(deserializeResult.m_entitiesLoaded, size_t{1});
        larvae::AssertEqual(deserializeResult.m_componentsLoaded, size_t{0});
        larvae::AssertEqual(dst.EntityCount(), size_t{1});
    });

    auto test_serialize_buffer_overflow_detected =
        larvae::RegisterTest("QueenWorldSerialization", "SerializeBufferOverflowDetected", []() {
            queen::ComponentRegistry<32> registry;
            registry.Register<Pos>();

            queen::World src;
            for (int i = 0; i < 64; ++i)
            {
                static_cast<void>(src.Spawn(Pos{static_cast<float>(i), static_cast<float>(i + 1), static_cast<float>(i + 2)}));
            }

            queen::WorldSerializer<128> serializer;
            const auto result = serializer.Serialize(src, registry);

            larvae::AssertFalse(result.m_success);
        });

    auto test_dynamic_serializer_large_world =
        larvae::RegisterTest("QueenWorldSerialization", "DynamicSerializerLargeWorld", []() {
            queen::ComponentRegistry<32> registry;
            registry.Register<Pos>();

            queen::World src;
            for (int i = 0; i < 512; ++i)
            {
                static_cast<void>(src.Spawn(Pos{static_cast<float>(i), static_cast<float>(i + 1), static_cast<float>(i + 2)}));
            }

            queen::DynamicWorldSerializer serializer;
            const auto serializeResult = serializer.Serialize(src, registry);

            larvae::AssertTrue(serializeResult.m_success);
            larvae::AssertEqual(serializeResult.m_entitiesWritten, size_t{512});
            larvae::AssertTrue(serializer.Size() > size_t{128});

            queen::World dst;
            const auto deserializeResult = queen::WorldDeserializer::Deserialize(dst, registry, serializer.CStr());

            larvae::AssertTrue(deserializeResult.m_success);
            larvae::AssertEqual(dst.EntityCount(), size_t{512});
            larvae::AssertTrue(HasEntityWithPos(dst, 511.f, 512.f, 513.f));
        });

    auto test_deserialize_result_counts =
        larvae::RegisterTest("QueenWorldSerialization", "DeserializeResultCounts", []() {
            queen::ComponentRegistry<32> registry;
            registry.Register<Pos>();
            registry.Register<Health>();

            queen::World src;
            static_cast<void>(src.Spawn(Pos{1.f, 0.f, 0.f}, Health{50, 100}));
            static_cast<void>(src.Spawn(Pos{2.f, 0.f, 0.f}));

            queen::WorldSerializer<8192> serializer;
            larvae::AssertTrue(serializer.Serialize(src, registry).m_success);

            queen::World dst;
            auto result = queen::WorldDeserializer::Deserialize(dst, registry, serializer.CStr());

            larvae::AssertTrue(result.m_success);
            larvae::AssertEqual(result.m_entitiesLoaded, size_t{2});
            larvae::AssertEqual(result.m_componentsLoaded, size_t{3}); // Pos + Health + Pos
            larvae::AssertEqual(result.m_componentsSkipped, size_t{0});
        });
} // namespace
