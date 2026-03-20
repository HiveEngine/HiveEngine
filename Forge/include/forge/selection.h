#pragma once

#include <queen/core/entity.h>

#include <forge/forge_module.h>

#include <wax/containers/vector.h>

namespace forge
{
    class EditorSelection
    {
    public:
        void Select(queen::Entity entity);

        void Toggle(queen::Entity entity);

        void Clear();

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
            return m_selected.IsEmpty();
        }

    private:
        queen::Entity m_primary{};
        wax::Vector<queen::Entity> m_selected{forge::GetAllocator()};
    };
} // namespace forge
