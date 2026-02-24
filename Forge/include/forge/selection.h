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
        [[nodiscard]] queen::Entity Primary() const noexcept { return primary_; }
        [[nodiscard]] const std::vector<queen::Entity>& All() const noexcept { return selected_; }
        [[nodiscard]] bool IsEmpty() const noexcept { return selected_.empty(); }

    private:
        queen::Entity primary_{};
        std::vector<queen::Entity> selected_;
    };
}
