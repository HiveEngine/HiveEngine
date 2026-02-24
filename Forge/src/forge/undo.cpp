#include <forge/undo.h>

#include <queen/world/world.h>

#include <cstring>

namespace forge
{
    void UndoStack::PushSetField(queen::Entity entity, queen::TypeId type_id,
                                  uint16_t offset, uint16_t size,
                                  const void* before, const void* after)
    {
        redo_count_ = 0;

        uint32_t data_off = static_cast<uint32_t>(data_head_);
        data_head_ = (data_head_ + size * 2) % kMaxDataBytes;

        std::memcpy(&data_[data_off], before, size);
        std::memcpy(&data_[(data_off + size) % kMaxDataBytes], after, size);

        UndoCommand cmd{};
        cmd.entity = entity;
        cmd.type_id = type_id;
        cmd.field_offset = offset;
        cmd.field_size = size;
        cmd.data_offset = data_off;

        commands_[head_] = cmd;
        head_ = (head_ + 1) % kMaxCommands;
        if (count_ < kMaxCommands)
            ++count_;
    }

    queen::Entity UndoStack::Undo(queen::World& world)
    {
        if (count_ == 0) return {};

        --count_;
        head_ = (head_ + kMaxCommands - 1) % kMaxCommands;
        ++redo_count_;

        const auto& cmd = commands_[head_];
        void* comp = world.GetComponentRaw(cmd.entity, cmd.type_id);
        if (!comp) return cmd.entity;

        auto* dst = static_cast<std::byte*>(comp) + cmd.field_offset;
        std::memcpy(dst, &data_[cmd.data_offset], cmd.field_size);
        return cmd.entity;
    }

    queen::Entity UndoStack::Redo(queen::World& world)
    {
        if (redo_count_ == 0) return {};

        const auto& cmd = commands_[head_];
        head_ = (head_ + 1) % kMaxCommands;
        --redo_count_;
        ++count_;

        void* comp = world.GetComponentRaw(cmd.entity, cmd.type_id);
        if (!comp) return cmd.entity;

        auto* dst = static_cast<std::byte*>(comp) + cmd.field_offset;
        uint32_t after_offset = (cmd.data_offset + cmd.field_size) % kMaxDataBytes;
        std::memcpy(dst, &data_[after_offset], cmd.field_size);
        return cmd.entity;
    }
}
