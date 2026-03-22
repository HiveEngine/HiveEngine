#pragma once

#include <wax/containers/vector.h>

#include <queen/core/entity.h>

#include <forge/forge_module.h>

#include <cstdint>
#include <filesystem>

namespace forge
{
    enum class AssetType : uint8_t;

    enum class SelectionKind : uint8_t
    {
        NONE,
        ENTITY,
        ASSET,
    };

    class EditorSelection
    {
    public:
        void Select(queen::Entity entity);
        void Toggle(queen::Entity entity);
        void AddToSelection(queen::Entity entity);
        void SelectAsset(const std::filesystem::path& path, AssetType type);
        void Clear();

        [[nodiscard]] SelectionKind Kind() const noexcept
        {
            return m_kind;
        }

        [[nodiscard]] bool IsSelected(queen::Entity entity) const noexcept;
        [[nodiscard]] queen::Entity Primary() const noexcept
        {
            return m_primary;
        }
        [[nodiscard]] const wax::Vector<queen::Entity>& All() const noexcept
        {
            return m_selected;
        }
        [[nodiscard]] bool IsEmpty() const noexcept
        {
            return m_kind == SelectionKind::NONE;
        }

        [[nodiscard]] const std::filesystem::path& AssetPath() const noexcept
        {
            return m_assetPath;
        }
        [[nodiscard]] AssetType SelectedAssetType() const noexcept;

    private:
        SelectionKind m_kind{SelectionKind::NONE};

        queen::Entity m_primary{};
        wax::Vector<queen::Entity> m_selected{forge::GetAllocator()};

        std::filesystem::path m_assetPath;
        uint8_t m_assetType{0};
    };
} // namespace forge
