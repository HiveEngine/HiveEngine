#include <queen/world/world.h>

#include <forge/entity_inspector_helpers.h>

namespace forge
{
    void CommitIfChanged(SnapshotState& state, EditorUndoManager& undo, queen::World& world,
                         const void* current)
    {
        if (state.m_size == 0)
        {
            return;
        }
        if (std::memcmp(state.m_before.data(), current, state.m_size) != 0)
        {
            auto beforeData = std::make_shared<std::vector<std::byte>>(
                std::move(state.m_before));
            auto afterData = std::make_shared<std::vector<std::byte>>(
                static_cast<const std::byte*>(current),
                static_cast<const std::byte*>(current) + state.m_size);
            auto entity = state.m_entity;
            auto typeId = state.m_typeId;
            auto offset = state.m_offset;
            auto size = state.m_size;
            undo.Push(
                [&world, entity, typeId, offset, size, beforeData]() {
                    void* comp = world.GetComponentRaw(entity, typeId);
                    if (comp != nullptr)
                        std::memcpy(static_cast<std::byte*>(comp) + offset,
                                    beforeData->data(), size);
                },
                [&world, entity, typeId, offset, size, afterData]() {
                    void* comp = world.GetComponentRaw(entity, typeId);
                    if (comp != nullptr)
                        std::memcpy(static_cast<std::byte*>(comp) + offset,
                                    afterData->data(), size);
                });
        }
        state.m_size = 0;
    }

    bool AreFieldValuesUniform(queen::World& world, const wax::Vector<queen::Entity>& entities,
                               queen::TypeId typeId, uint16_t fieldOffset, uint16_t fieldSize)
    {
        if (entities.Size() <= 1)
            return true;

        void* firstComp = world.GetComponentRaw(entities[0], typeId);
        if (firstComp == nullptr)
            return true;

        auto* firstBytes = static_cast<std::byte*>(firstComp) + fieldOffset;

        for (size_t i = 1; i < entities.Size(); ++i)
        {
            void* comp = world.GetComponentRaw(entities[i], typeId);
            if (comp == nullptr)
                return false;

            auto* bytes = static_cast<std::byte*>(comp) + fieldOffset;
            if (std::memcmp(firstBytes, bytes, fieldSize) != 0)
                return false;
        }
        return true;
    }
} // namespace forge
