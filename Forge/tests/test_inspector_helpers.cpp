#include <queen/world/world.h>

#include <forge/editor_undo.h>
#include <forge/entity_inspector_helpers.h>

#include <larvae/larvae.h>

#include <cstring>

namespace
{
    struct TestComp
    {
        float x{0.f};
        float y{0.f};
        float z{0.f};
        int32_t id{0};
    };

    queen::TypeId TestTypeId()
    {
        return queen::TypeIdOf<TestComp>();
    }

    // --- Snapshot / CommitIfChanged ---

    auto t_snapshot_captures_data = larvae::RegisterTest("InspectorHelpers", "SnapshotCapturesData", []() {
        float value = 42.5f;
        forge::SnapshotState state;
        queen::Entity e{1, 1};

        forge::Snapshot(state, e, TestTypeId(), 0, sizeof(float), &value);

        larvae::AssertEqual(state.m_size, uint16_t{sizeof(float)});
        larvae::AssertEqual(state.m_entity.Index(), 1u);

        float captured = 0.f;
        std::memcpy(&captured, state.m_before.data(), sizeof(float));
        larvae::AssertEqual(captured, 42.5f);
    });

    auto t_commit_no_change = larvae::RegisterTest("InspectorHelpers", "CommitNoChangeDoesNotPush", []() {
        queen::World world;
        forge::EditorUndoManager undo;
        queen::Entity e = world.Spawn(TestComp{5.f, 0.f, 0.f, 0});

        auto* comp = world.Get<TestComp>(e);
        forge::SnapshotState state;
        forge::Snapshot(state, e, TestTypeId(), 0, sizeof(float), &comp->x);

        forge::CommitIfChanged(state, undo, world, &comp->x);

        larvae::AssertFalse(undo.CanUndo());
    });

    auto t_commit_with_change = larvae::RegisterTest("InspectorHelpers", "CommitWithChangePushesUndo", []() {
        queen::World world;
        forge::EditorUndoManager undo;
        queen::Entity e = world.Spawn(TestComp{5.f, 0.f, 0.f, 0});

        auto* comp = world.Get<TestComp>(e);
        forge::SnapshotState state;
        forge::Snapshot(state, e, TestTypeId(), 0, sizeof(float), &comp->x);

        comp->x = 10.f;
        forge::CommitIfChanged(state, undo, world, &comp->x);

        larvae::AssertTrue(undo.CanUndo());
        larvae::AssertEqual(comp->x, 10.f);

        undo.Undo();
        larvae::AssertEqual(comp->x, 5.f);

        undo.Redo();
        larvae::AssertEqual(comp->x, 10.f);
    });

    auto t_commit_resets_size = larvae::RegisterTest("InspectorHelpers", "CommitResetsState", []() {
        queen::World world;
        forge::EditorUndoManager undo;
        queen::Entity e = world.Spawn(TestComp{1.f, 0.f, 0.f, 0});

        auto* comp = world.Get<TestComp>(e);
        forge::SnapshotState state;
        forge::Snapshot(state, e, TestTypeId(), 0, sizeof(float), &comp->x);
        comp->x = 2.f;
        forge::CommitIfChanged(state, undo, world, &comp->x);

        larvae::AssertEqual(state.m_size, uint16_t{0});
    });

    auto t_commit_large_field = larvae::RegisterTest("InspectorHelpers", "CommitLargeFieldWorks", []() {
        queen::World world;
        forge::EditorUndoManager undo;
        queen::Entity e = world.Spawn(TestComp{1.f, 2.f, 3.f, 42});

        auto* comp = world.Get<TestComp>(e);
        forge::SnapshotState state;
        forge::Snapshot(state, e, TestTypeId(), 0, sizeof(TestComp), comp);

        comp->x = 10.f;
        comp->y = 20.f;
        comp->z = 30.f;
        comp->id = 99;
        forge::CommitIfChanged(state, undo, world, comp);

        larvae::AssertTrue(undo.CanUndo());

        undo.Undo();
        larvae::AssertEqual(comp->x, 1.f);
        larvae::AssertEqual(comp->y, 2.f);
        larvae::AssertEqual(comp->z, 3.f);
        larvae::AssertEqual(comp->id, 42);
    });

    // --- AreFieldValuesUniform ---

    auto t_uniform_single = larvae::RegisterTest("InspectorHelpers", "UniformSingleEntityIsTrue", []() {
        queen::World world;
        queen::Entity e = world.Spawn(TestComp{5.f, 0.f, 0.f, 0});

        wax::Vector<queen::Entity> entities{comb::GetDefaultAllocator()};
        entities.PushBack(e);

        larvae::AssertTrue(
            forge::AreFieldValuesUniform(world, entities, TestTypeId(), 0, sizeof(float)));
    });

    auto t_uniform_same_values = larvae::RegisterTest("InspectorHelpers", "UniformSameValuesIsTrue", []() {
        queen::World world;
        queen::Entity e1 = world.Spawn(TestComp{5.f, 0.f, 0.f, 0});
        queen::Entity e2 = world.Spawn(TestComp{5.f, 0.f, 0.f, 0});
        queen::Entity e3 = world.Spawn(TestComp{5.f, 0.f, 0.f, 0});

        wax::Vector<queen::Entity> entities{comb::GetDefaultAllocator()};
        entities.PushBack(e1);
        entities.PushBack(e2);
        entities.PushBack(e3);

        larvae::AssertTrue(
            forge::AreFieldValuesUniform(world, entities, TestTypeId(), 0, sizeof(float)));
    });

    auto t_uniform_different = larvae::RegisterTest("InspectorHelpers", "UniformDifferentValuesIsFalse", []() {
        queen::World world;
        queen::Entity e1 = world.Spawn(TestComp{5.f, 0.f, 0.f, 0});
        queen::Entity e2 = world.Spawn(TestComp{7.f, 0.f, 0.f, 0});

        wax::Vector<queen::Entity> entities{comb::GetDefaultAllocator()};
        entities.PushBack(e1);
        entities.PushBack(e2);

        larvae::AssertFalse(
            forge::AreFieldValuesUniform(world, entities, TestTypeId(), 0, sizeof(float)));
    });

    auto t_uniform_different_field = larvae::RegisterTest("InspectorHelpers", "UniformChecksCorrectOffset", []() {
        queen::World world;
        queen::Entity e1 = world.Spawn(TestComp{1.f, 5.f, 0.f, 0});
        queen::Entity e2 = world.Spawn(TestComp{2.f, 5.f, 0.f, 0});

        wax::Vector<queen::Entity> entities{comb::GetDefaultAllocator()};
        entities.PushBack(e1);
        entities.PushBack(e2);

        // x differs
        larvae::AssertFalse(
            forge::AreFieldValuesUniform(world, entities, TestTypeId(), 0, sizeof(float)));
        // y is uniform
        larvae::AssertTrue(forge::AreFieldValuesUniform(
            world, entities, TestTypeId(), static_cast<uint16_t>(offsetof(TestComp, y)), sizeof(float)));
    });

    // --- ApplyMultiEdit ---

    auto t_multi_edit_applies = larvae::RegisterTest("InspectorHelpers", "ApplyMultiEditChangesAll", []() {
        queen::World world;
        forge::EditorUndoManager undo;
        queen::Entity e1 = world.Spawn(TestComp{1.f, 0.f, 0.f, 0});
        queen::Entity e2 = world.Spawn(TestComp{2.f, 0.f, 0.f, 0});
        queen::Entity e3 = world.Spawn(TestComp{3.f, 0.f, 0.f, 0});

        wax::Vector<queen::Entity> entities{comb::GetDefaultAllocator()};
        entities.PushBack(e1);
        entities.PushBack(e2);
        entities.PushBack(e3);

        forge::ApplyMultiEdit<float>(world, entities, TestTypeId(), 0, 99.f, undo);

        larvae::AssertEqual(world.Get<TestComp>(e1)->x, 99.f);
        larvae::AssertEqual(world.Get<TestComp>(e2)->x, 99.f);
        larvae::AssertEqual(world.Get<TestComp>(e3)->x, 99.f);
    });

    auto t_multi_edit_undo = larvae::RegisterTest("InspectorHelpers", "ApplyMultiEditUndoRestores", []() {
        queen::World world;
        forge::EditorUndoManager undo;
        queen::Entity e1 = world.Spawn(TestComp{1.f, 0.f, 0.f, 0});
        queen::Entity e2 = world.Spawn(TestComp{2.f, 0.f, 0.f, 0});

        wax::Vector<queen::Entity> entities{comb::GetDefaultAllocator()};
        entities.PushBack(e1);
        entities.PushBack(e2);

        forge::ApplyMultiEdit<float>(world, entities, TestTypeId(), 0, 50.f, undo);

        larvae::AssertTrue(undo.CanUndo());
        undo.Undo();

        larvae::AssertEqual(world.Get<TestComp>(e1)->x, 1.f);
        larvae::AssertEqual(world.Get<TestComp>(e2)->x, 2.f);
    });

    auto t_multi_edit_redo = larvae::RegisterTest("InspectorHelpers", "ApplyMultiEditRedoReapplies", []() {
        queen::World world;
        forge::EditorUndoManager undo;
        queen::Entity e1 = world.Spawn(TestComp{1.f, 0.f, 0.f, 0});
        queen::Entity e2 = world.Spawn(TestComp{2.f, 0.f, 0.f, 0});

        wax::Vector<queen::Entity> entities{comb::GetDefaultAllocator()};
        entities.PushBack(e1);
        entities.PushBack(e2);

        forge::ApplyMultiEdit<float>(world, entities, TestTypeId(), 0, 50.f, undo);
        undo.Undo();
        undo.Redo();

        larvae::AssertEqual(world.Get<TestComp>(e1)->x, 50.f);
        larvae::AssertEqual(world.Get<TestComp>(e2)->x, 50.f);
    });

    auto t_multi_edit_int = larvae::RegisterTest("InspectorHelpers", "ApplyMultiEditInt32", []() {
        queen::World world;
        forge::EditorUndoManager undo;
        queen::Entity e1 = world.Spawn(TestComp{0.f, 0.f, 0.f, 10});
        queen::Entity e2 = world.Spawn(TestComp{0.f, 0.f, 0.f, 20});

        wax::Vector<queen::Entity> entities{comb::GetDefaultAllocator()};
        entities.PushBack(e1);
        entities.PushBack(e2);

        forge::ApplyMultiEdit<int32_t>(
            world, entities, TestTypeId(),
            static_cast<uint16_t>(offsetof(TestComp, id)), 42, undo);

        larvae::AssertEqual(world.Get<TestComp>(e1)->id, 42);
        larvae::AssertEqual(world.Get<TestComp>(e2)->id, 42);

        undo.Undo();
        larvae::AssertEqual(world.Get<TestComp>(e1)->id, 10);
        larvae::AssertEqual(world.Get<TestComp>(e2)->id, 20);
    });

    // --- EditorUndoManager ---

    auto t_undo_manager_empty = larvae::RegisterTest("InspectorHelpers", "UndoManagerEmptyByDefault", []() {
        forge::EditorUndoManager undo;
        larvae::AssertFalse(undo.CanUndo());
        larvae::AssertFalse(undo.CanRedo());
    });

    auto t_undo_manager_push_undo_redo = larvae::RegisterTest("InspectorHelpers", "UndoManagerPushUndoRedo", []() {
        forge::EditorUndoManager undo;
        int value = 0;

        undo.Push([&value]() { value = 0; }, [&value]() { value = 1; });
        value = 1;

        larvae::AssertTrue(undo.CanUndo());
        larvae::AssertFalse(undo.CanRedo());

        undo.Undo();
        larvae::AssertEqual(value, 0);
        larvae::AssertFalse(undo.CanUndo());
        larvae::AssertTrue(undo.CanRedo());

        undo.Redo();
        larvae::AssertEqual(value, 1);
    });

    auto t_undo_manager_push_clears_redo = larvae::RegisterTest("InspectorHelpers", "PushClearsRedoStack", []() {
        forge::EditorUndoManager undo;
        int value = 0;

        undo.Push([&value]() { value = 0; }, [&value]() { value = 1; });
        value = 1;
        undo.Undo();

        larvae::AssertTrue(undo.CanRedo());

        undo.Push([&value]() { value = 0; }, [&value]() { value = 2; });
        larvae::AssertFalse(undo.CanRedo());
    });

    auto t_undo_manager_multiple = larvae::RegisterTest("InspectorHelpers", "UndoManagerMultipleOps", []() {
        forge::EditorUndoManager undo;
        int value = 0;

        undo.Push([&value]() { value = 0; }, [&value]() { value = 1; });
        value = 1;
        undo.Push([&value]() { value = 1; }, [&value]() { value = 2; });
        value = 2;
        undo.Push([&value]() { value = 2; }, [&value]() { value = 3; });
        value = 3;

        undo.Undo();
        larvae::AssertEqual(value, 2);
        undo.Undo();
        larvae::AssertEqual(value, 1);
        undo.Undo();
        larvae::AssertEqual(value, 0);
        larvae::AssertFalse(undo.CanUndo());

        undo.Redo();
        larvae::AssertEqual(value, 1);
        undo.Redo();
        larvae::AssertEqual(value, 2);
    });
} // namespace
