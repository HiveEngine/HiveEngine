#pragma once

#include <comb/allocator_concepts.h>

#include <wax/containers/vector.h>

#include <queen/core/entity.h>

namespace queen
{
    // Cached child entity list, automatically managed by the hierarchy system.
    // Do not modify directly — use World::SetParent / World::RemoveParent.
    template <comb::Allocator Allocator> class ChildrenT
    {
    public:
        explicit ChildrenT(Allocator& alloc)
            : m_entities{alloc}
        {
        }

        ChildrenT(const ChildrenT&) = delete;
        ChildrenT& operator=(const ChildrenT&) = delete;

        ChildrenT(ChildrenT&& other) noexcept = default;
        ChildrenT& operator=(ChildrenT&& other) noexcept = default;

        void Add(Entity child)
        {
            m_entities.PushBack(child);
        }

        bool Remove(Entity child)
        {
            for (size_t i = 0; i < m_entities.Size(); ++i)
            {
                if (m_entities[i] == child)
                {
                    for (size_t j = i; j + 1 < m_entities.Size(); ++j)
                    {
                        m_entities[j] = m_entities[j + 1];
                    }
                    m_entities.PopBack();
                    return true;
                }
            }
            return false;
        }

        void InsertAt(size_t index, Entity child)
        {
            if (index >= m_entities.Size())
            {
                m_entities.PushBack(child);
                return;
            }
            m_entities.PushBack(m_entities.Back());
            for (size_t i = m_entities.Size() - 1; i > index; --i)
            {
                m_entities[i] = m_entities[i - 1];
            }
            m_entities[index] = child;
        }

        [[nodiscard]] size_t IndexOf(Entity child) const
        {
            for (size_t i = 0; i < m_entities.Size(); ++i)
            {
                if (m_entities[i] == child)
                    return i;
            }
            return m_entities.Size();
        }

        [[nodiscard]] bool Contains(Entity child) const
        {
            for (size_t i = 0; i < m_entities.Size(); ++i)
            {
                if (m_entities[i] == child)
                {
                    return true;
                }
            }
            return false;
        }

        [[nodiscard]] Entity At(size_t index) const
        {
            return m_entities[index];
        }

        [[nodiscard]] size_t Count() const noexcept
        {
            return m_entities.Size();
        }

        [[nodiscard]] bool IsEmpty() const noexcept
        {
            return m_entities.IsEmpty();
        }

        // Iterator support

        [[nodiscard]] const Entity* Begin() const noexcept
        {
            return m_entities.Begin();
        }

        [[nodiscard]] const Entity* End() const noexcept
        {
            return m_entities.End();
        }

        [[nodiscard]] Entity* Begin() noexcept
        {
            return m_entities.Begin();
        }

        [[nodiscard]] Entity* End() noexcept
        {
            return m_entities.End();
        }

    private:
        wax::Vector<Entity> m_entities;
    };
} // namespace queen
