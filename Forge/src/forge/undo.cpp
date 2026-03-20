#include <forge/undo.h>

#include <queen/world/world.h>

#include <cstring>

namespace forge
{
    namespace
    {
        void RingWrite(std::byte* ring, size_t cap, size_t offset, const void* src, size_t size)
        {
            auto* s = static_cast<const std::byte*>(src);
            size_t first = cap - offset;
            if (first >= size)
            {
                std::memcpy(&ring[offset], s, size);
            }
            else
            {
                std::memcpy(&ring[offset], s, first);
                std::memcpy(&ring[0], s + first, size - first);
            }
        }

        void RingRead(const std::byte* ring, size_t cap, size_t offset, void* dst, size_t size)
        {
            auto* d = static_cast<std::byte*>(dst);
            size_t first = cap - offset;
            if (first >= size)
            {
                std::memcpy(d, &ring[offset], size);
            }
            else
            {
                std::memcpy(d, &ring[offset], first);
                std::memcpy(d + first, &ring[0], size - first);
            }
        }
    } // namespace

    void UndoStack::PushSetField(queen::Entity entity, queen::TypeId typeId, uint16_t offset, uint16_t size,
                                 const void* before, const void* after)
    {
        m_redoCount = 0;

        uint32_t dataOff = static_cast<uint32_t>(m_dataHead);
        m_dataHead = (m_dataHead + size * 2) % kMaxDataBytes;

        RingWrite(m_data, kMaxDataBytes, dataOff, before, size);
        RingWrite(m_data, kMaxDataBytes, (dataOff + size) % kMaxDataBytes, after, size);

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
        RingRead(m_data, kMaxDataBytes, cmd.m_dataOffset, dst, cmd.m_fieldSize);
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
        RingRead(m_data, kMaxDataBytes, afterOffset, dst, cmd.m_fieldSize);
        return cmd.m_entity;
    }
} // namespace forge
