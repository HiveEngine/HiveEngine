#pragma once

#include <queen/core/entity.h>

#include <vector>

namespace forge
{
    class EditorSelection
    {
    public:
        // Replace selection with a single entity
        void Select(queen::Entity entity);

        // Toggle entity in selection (Ctrl+click)
        void Toggle(queen::Entity entity);

        void Clear();

        [[nodiscard]] bool IsSelected(queen::Entity entity) const noexcept;
        [[nodiscard]] queen::Entity Primary() const noexcept
        {
            return m_primary;
        }
        [[nodiscard]] const std::vector<queen::Entity>& All() const noexcept
        {
            return m_selected;
        }
        [[nodiscard]] bool IsEmpty() const noexcept
        {
            return m_selected.empty();
        }

    private:
        queen::Entity m_primary{};
        std::vector<queen::Entity> m_selected;
    };
} // namespace forge
