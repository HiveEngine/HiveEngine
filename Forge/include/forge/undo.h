#pragma once

#include <queen/core/entity.h>
#include <queen/core/type_id.h>

#include <cstddef>
#include <cstdint>

namespace queen { class World; }

namespace forge
{
    struct UndoCommand
    {
        queen::Entity entity{};
        queen::TypeId type_id{0};
        uint16_t field_offset{0};
        uint16_t field_size{0};
        uint32_t data_offset{0};     // offset into data buffer (before then after)
    };

    // Simple undo/redo stack with fixed-size ring buffer.
    class UndoStack
    {
    public:
        static constexpr size_t kMaxCommands = 1024;
        static constexpr size_t kMaxDataBytes = 4 * 1024 * 1024; // 4MB

        void PushSetField(queen::Entity entity, queen::TypeId type_id,
                          uint16_t offset, uint16_t size,
                          const void* before, const void* after);

        queen::Entity Undo(queen::World& world);
        queen::Entity Redo(queen::World& world);

        [[nodiscard]] bool CanUndo() const noexcept { return count_ > 0; }
        [[nodiscard]] bool CanRedo() const noexcept { return redo_count_ > 0; }
        [[nodiscard]] size_t Count() const noexcept { return count_; }

    private:
        UndoCommand commands_[kMaxCommands]{};
        std::byte data_[kMaxDataBytes]{};

        size_t head_{0};
        size_t count_{0};
        size_t redo_count_{0};
        size_t data_head_{0};
    };
}
