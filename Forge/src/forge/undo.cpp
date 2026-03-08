#include <queen/world/world.h>

#include <forge/undo.h>

#include <cstring>

namespace forge
{
    void UndoStack::PushSetField(queen::Entity entity, queen::TypeId typeId, uint16_t offset, uint16_t size,
                                 const void* before, const void* after)
    {
        m_redoCount = 0;

        uint32_t dataOff = static_cast<uint32_t>(m_dataHead);
        m_dataHead = (m_dataHead + size * 2) % kMaxDataBytes;

        std::memcpy(&m_data[dataOff], before, size);
        std::memcpy(&m_data[(dataOff + size) % kMaxDataBytes], after, size);

        UndoCommand cmd{};
        cmd.m_entity = entity;
        cmd.m_typeId = typeId;
        cmd.m_fieldOffset = offset;
        cmd.m_fieldSize = size;
        cmd.m_dataOffset = dataOff;

        m_commands[m_head] = cmd;
        m_head = (m_head + 1) % kMaxCommands;
        if (m_count < kMaxCommands)
            ++m_count;
    }

    queen::Entity UndoStack::Undo(queen::World& world)
    {
        if (m_count == 0)
            return {};

        --m_count;
        m_head = (m_head + kMaxCommands - 1) % kMaxCommands;
        ++m_redoCount;

        const auto& cmd = m_commands[m_head];
        void* comp = world.GetComponentRaw(cmd.m_entity, cmd.m_typeId);
        if (!comp)
            return cmd.m_entity;

        auto* dst = static_cast<std::byte*>(comp) + cmd.m_fieldOffset;
        std::memcpy(dst, &m_data[cmd.m_dataOffset], cmd.m_fieldSize);
        return cmd.m_entity;
    }

    queen::Entity UndoStack::Redo(queen::World& world)
    {
        if (m_redoCount == 0)
            return {};

        const auto& cmd = m_commands[m_head];
        m_head = (m_head + 1) % kMaxCommands;
        --m_redoCount;
        ++m_count;

        void* comp = world.GetComponentRaw(cmd.m_entity, cmd.m_typeId);
        if (!comp)
            return cmd.m_entity;

        auto* dst = static_cast<std::byte*>(comp) + cmd.m_fieldOffset;
        uint32_t afterOffset = (cmd.m_dataOffset + cmd.m_fieldSize) % kMaxDataBytes;
        std::memcpy(dst, &m_data[afterOffset], cmd.m_fieldSize);
        return cmd.m_entity;
    }
} // namespace forge
