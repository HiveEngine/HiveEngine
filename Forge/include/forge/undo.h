#pragma once

#include <queen/core/entity.h>
#include <queen/core/type_id.h>

#include <cstddef>
#include <cstdint>

namespace queen
{
    class World;
}

namespace forge
{
    struct UndoCommand
    {
        queen::Entity m_entity{};
        queen::TypeId m_typeId{0};
        uint16_t m_fieldOffset{0};
        uint16_t m_fieldSize{0};
        uint32_t m_dataOffset{0}; // offset into data buffer (before then after)
    };

    // Simple undo/redo stack with fixed-size ring buffer.
    class UndoStack
    {
    public:
        static constexpr size_t kMaxCommands = 1024;
        static constexpr size_t kMaxDataBytes = 4 * 1024 * 1024; // 4MB

        void PushSetField(queen::Entity entity, queen::TypeId typeId, uint16_t offset, uint16_t size,
                          const void* before, const void* after);

        queen::Entity Undo(queen::World& world);
        queen::Entity Redo(queen::World& world);

        [[nodiscard]] bool CanUndo() const noexcept
        {
            return m_count > 0;
        }
        [[nodiscard]] bool CanRedo() const noexcept
        {
            return m_redoCount > 0;
        }
        [[nodiscard]] size_t Count() const noexcept
        {
            return m_count;
        }

    private:
        UndoCommand m_commands[kMaxCommands]{};
        std::byte m_data[kMaxDataBytes]{};

        size_t m_head{0};
        size_t m_count{0};
        size_t m_redoCount{0};
        size_t m_dataHead{0};
    };
} // namespace forge
