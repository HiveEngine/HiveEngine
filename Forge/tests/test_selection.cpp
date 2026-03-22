#include <forge/asset_browser.h>
#include <forge/selection.h>

#include <larvae/larvae.h>

namespace
{
    queen::Entity MakeEntity(uint32_t index)
    {
        return queen::Entity{index, 1};
    }

    auto t_empty_by_default = larvae::RegisterTest("EditorSelection", "EmptyByDefault", []() {
        forge::EditorSelection sel;
        larvae::AssertTrue(sel.IsEmpty());
        larvae::AssertEqual(static_cast<uint8_t>(sel.Kind()), static_cast<uint8_t>(forge::SelectionKind::NONE));
        larvae::AssertTrue(sel.Primary().IsNull());
        larvae::AssertEqual(sel.All().Size(), size_t{0});
    });

    auto t_select_single = larvae::RegisterTest("EditorSelection", "SelectSetsEntity", []() {
        forge::EditorSelection sel;
        auto e = MakeEntity(5);
        sel.Select(e);

        larvae::AssertFalse(sel.IsEmpty());
        larvae::AssertEqual(static_cast<uint8_t>(sel.Kind()), static_cast<uint8_t>(forge::SelectionKind::ENTITY));
        larvae::AssertEqual(sel.Primary().Index(), uint32_t{5});
        larvae::AssertEqual(sel.All().Size(), size_t{1});
        larvae::AssertTrue(sel.IsSelected(e));
    });

    auto t_select_replaces = larvae::RegisterTest("EditorSelection", "SelectReplacesPrevious", []() {
        forge::EditorSelection sel;
        sel.Select(MakeEntity(1));
        sel.Select(MakeEntity(2));

        larvae::AssertEqual(sel.All().Size(), size_t{1});
        larvae::AssertEqual(sel.Primary().Index(), uint32_t{2});
        larvae::AssertFalse(sel.IsSelected(MakeEntity(1)));
        larvae::AssertTrue(sel.IsSelected(MakeEntity(2)));
    });

    auto t_select_null_clears = larvae::RegisterTest("EditorSelection", "SelectNullClears", []() {
        forge::EditorSelection sel;
        sel.Select(MakeEntity(1));
        sel.Select(queen::Entity{});

        larvae::AssertTrue(sel.IsEmpty());
        larvae::AssertEqual(sel.All().Size(), size_t{0});
    });

    auto t_toggle_adds = larvae::RegisterTest("EditorSelection", "ToggleAddsEntity", []() {
        forge::EditorSelection sel;
        sel.Select(MakeEntity(1));
        sel.Toggle(MakeEntity(2));

        larvae::AssertEqual(sel.All().Size(), size_t{2});
        larvae::AssertTrue(sel.IsSelected(MakeEntity(1)));
        larvae::AssertTrue(sel.IsSelected(MakeEntity(2)));
        larvae::AssertEqual(sel.Primary().Index(), uint32_t{2});
    });

    auto t_toggle_removes = larvae::RegisterTest("EditorSelection", "ToggleRemovesIfPresent", []() {
        forge::EditorSelection sel;
        sel.Select(MakeEntity(1));
        sel.Toggle(MakeEntity(2));
        sel.Toggle(MakeEntity(1));

        larvae::AssertEqual(sel.All().Size(), size_t{1});
        larvae::AssertFalse(sel.IsSelected(MakeEntity(1)));
        larvae::AssertTrue(sel.IsSelected(MakeEntity(2)));
    });

    auto t_toggle_removes_last = larvae::RegisterTest("EditorSelection", "ToggleLastEntityClearsSelection", []() {
        forge::EditorSelection sel;
        sel.Select(MakeEntity(1));
        sel.Toggle(MakeEntity(1));

        larvae::AssertTrue(sel.IsEmpty());
        larvae::AssertTrue(sel.Primary().IsNull());
    });

    auto t_toggle_null_noop = larvae::RegisterTest("EditorSelection", "ToggleNullIsNoop", []() {
        forge::EditorSelection sel;
        sel.Select(MakeEntity(1));
        sel.Toggle(queen::Entity{});

        larvae::AssertEqual(sel.All().Size(), size_t{1});
    });

    auto t_toggle_updates_primary = larvae::RegisterTest("EditorSelection", "TogglePrimaryFallsBackToLast", []() {
        forge::EditorSelection sel;
        sel.Select(MakeEntity(1));
        sel.Toggle(MakeEntity(2));
        sel.Toggle(MakeEntity(3));

        larvae::AssertEqual(sel.Primary().Index(), uint32_t{3});

        sel.Toggle(MakeEntity(3));
        larvae::AssertEqual(sel.Primary().Index(), uint32_t{2});
    });

    auto t_add_no_duplicates = larvae::RegisterTest("EditorSelection", "AddToSelectionNoDuplicates", []() {
        forge::EditorSelection sel;
        sel.Select(MakeEntity(1));
        sel.AddToSelection(MakeEntity(1));

        larvae::AssertEqual(sel.All().Size(), size_t{1});
    });

    auto t_add_appends = larvae::RegisterTest("EditorSelection", "AddToSelectionAppends", []() {
        forge::EditorSelection sel;
        sel.Select(MakeEntity(1));
        sel.AddToSelection(MakeEntity(2));
        sel.AddToSelection(MakeEntity(3));

        larvae::AssertEqual(sel.All().Size(), size_t{3});
        larvae::AssertTrue(sel.IsSelected(MakeEntity(1)));
        larvae::AssertTrue(sel.IsSelected(MakeEntity(2)));
        larvae::AssertTrue(sel.IsSelected(MakeEntity(3)));
        larvae::AssertEqual(sel.Primary().Index(), uint32_t{3});
    });

    auto t_add_null_noop = larvae::RegisterTest("EditorSelection", "AddToSelectionNullIsNoop", []() {
        forge::EditorSelection sel;
        sel.AddToSelection(queen::Entity{});
        larvae::AssertTrue(sel.IsEmpty());
    });

    auto t_clear = larvae::RegisterTest("EditorSelection", "ClearResetsEverything", []() {
        forge::EditorSelection sel;
        sel.Select(MakeEntity(1));
        sel.Toggle(MakeEntity(2));
        sel.Clear();

        larvae::AssertTrue(sel.IsEmpty());
        larvae::AssertTrue(sel.Primary().IsNull());
        larvae::AssertEqual(sel.All().Size(), size_t{0});
    });

    auto t_select_asset = larvae::RegisterTest("EditorSelection", "SelectAssetSwitchesKind", []() {
        forge::EditorSelection sel;
        sel.Select(MakeEntity(1));
        sel.SelectAsset(std::filesystem::path{"textures/foo.png"}, forge::AssetType::TEXTURE);

        larvae::AssertEqual(static_cast<uint8_t>(sel.Kind()), static_cast<uint8_t>(forge::SelectionKind::ASSET));
        larvae::AssertEqual(sel.All().Size(), size_t{0});
        larvae::AssertTrue(sel.Primary().IsNull());
        larvae::AssertEqual(sel.AssetPath().filename().string(), std::string{"foo.png"});
        larvae::AssertEqual(static_cast<uint8_t>(sel.SelectedAssetType()),
                            static_cast<uint8_t>(forge::AssetType::TEXTURE));
    });

    auto t_select_entity_clears_asset = larvae::RegisterTest("EditorSelection", "SelectEntityClearsAsset", []() {
        forge::EditorSelection sel;
        sel.SelectAsset(std::filesystem::path{"mesh.obj"}, forge::AssetType::MESH);
        sel.Select(MakeEntity(1));

        larvae::AssertEqual(static_cast<uint8_t>(sel.Kind()), static_cast<uint8_t>(forge::SelectionKind::ENTITY));
        larvae::AssertTrue(sel.AssetPath().empty());
    });

    auto t_toggle_clears_asset = larvae::RegisterTest("EditorSelection", "ToggleClearsAssetPath", []() {
        forge::EditorSelection sel;
        sel.SelectAsset(std::filesystem::path{"mesh.obj"}, forge::AssetType::MESH);
        sel.Toggle(MakeEntity(1));

        larvae::AssertEqual(static_cast<uint8_t>(sel.Kind()), static_cast<uint8_t>(forge::SelectionKind::ENTITY));
        larvae::AssertTrue(sel.AssetPath().empty());
    });

    auto t_add_clears_asset = larvae::RegisterTest("EditorSelection", "AddToSelectionClearsAssetPath", []() {
        forge::EditorSelection sel;
        sel.SelectAsset(std::filesystem::path{"mesh.obj"}, forge::AssetType::MESH);
        sel.AddToSelection(MakeEntity(1));

        larvae::AssertEqual(static_cast<uint8_t>(sel.Kind()), static_cast<uint8_t>(forge::SelectionKind::ENTITY));
        larvae::AssertTrue(sel.AssetPath().empty());
    });

    auto t_multiselect_workflow = larvae::RegisterTest("EditorSelection", "TypicalMultiselectWorkflow", []() {
        forge::EditorSelection sel;

        sel.Select(MakeEntity(1));
        larvae::AssertEqual(sel.All().Size(), size_t{1});

        sel.Toggle(MakeEntity(2));
        sel.Toggle(MakeEntity(3));
        larvae::AssertEqual(sel.All().Size(), size_t{3});

        sel.Toggle(MakeEntity(2));
        larvae::AssertEqual(sel.All().Size(), size_t{2});
        larvae::AssertFalse(sel.IsSelected(MakeEntity(2)));

        sel.Select(MakeEntity(10));
        larvae::AssertEqual(sel.All().Size(), size_t{1});
        larvae::AssertEqual(sel.Primary().Index(), uint32_t{10});
    });

    auto t_range_via_add = larvae::RegisterTest("EditorSelection", "RangeSelectionViaAdd", []() {
        forge::EditorSelection sel;

        sel.Clear();
        for (uint32_t i = 5; i <= 10; ++i)
        {
            sel.AddToSelection(MakeEntity(i));
        }

        larvae::AssertEqual(sel.All().Size(), size_t{6});
        for (uint32_t i = 5; i <= 10; ++i)
        {
            larvae::AssertTrue(sel.IsSelected(MakeEntity(i)));
        }
        larvae::AssertEqual(sel.Primary().Index(), uint32_t{10});
    });
} // namespace
