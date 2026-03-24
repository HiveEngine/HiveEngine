#pragma once

#include <queen/core/entity.h>
#include <queen/core/type_id.h>
#include <queen/world/world.h>

#include <wax/containers/vector.h>

#include <forge/editor_undo.h>

#include <cstddef>
#include <cstring>
#include <memory>
#include <vector>

namespace queen
{
    class World;
}

namespace forge
{
    struct SnapshotState
    {
        std::vector<std::byte> m_before;
        uint16_t m_size{0};
        queen::Entity m_entity{};
        queen::TypeId m_typeId{0};
        uint16_t m_offset{0};
    };

    inline void Snapshot(SnapshotState& state, queen::Entity entity, queen::TypeId typeId,
                         uint16_t offset, uint16_t size, const void* current)
    {
        state.m_entity = entity;
        state.m_typeId = typeId;
        state.m_offset = offset;
        state.m_size = size;
        state.m_before.resize(size);
        std::memcpy(state.m_before.data(), current, size);
    }

    void CommitIfChanged(SnapshotState& state, EditorUndoManager& undo, queen::World& world,
                         const void* current);

    bool AreFieldValuesUniform(queen::World& world, const wax::Vector<queen::Entity>& entities,
                               queen::TypeId typeId, uint16_t fieldOffset, uint16_t fieldSize);

    template <typename T>
    void ApplyMultiEdit(queen::World& world, const wax::Vector<queen::Entity>& entities,
                        queen::TypeId typeId, uint16_t fieldOffset, T newValue,
                        EditorUndoManager& editorUndo)
    {
        auto beforeSnapshots = std::make_shared<std::vector<std::pair<queen::Entity, T>>>();
        for (size_t i = 0; i < entities.Size(); ++i)
        {
            void* comp = world.GetComponentRaw(entities[i], typeId);
            if (comp == nullptr)
                continue;
            auto* ptr = reinterpret_cast<T*>(static_cast<std::byte*>(comp) + fieldOffset);
            beforeSnapshots->push_back({entities[i], *ptr});
            *ptr = newValue;
        }

        auto capturedEntities = std::make_shared<wax::Vector<queen::Entity>>(entities);
        editorUndo.Push(
            [&world, typeId, fieldOffset, beforeSnapshots] {
                for (auto& [e, before] : *beforeSnapshots)
                {
                    void* comp = world.GetComponentRaw(e, typeId);
                    if (comp != nullptr)
                        *reinterpret_cast<T*>(static_cast<std::byte*>(comp) + fieldOffset) = before;
                }
            },
            [&world, typeId, fieldOffset, newValue, capturedEntities] {
                for (size_t i = 0; i < capturedEntities->Size(); ++i)
                {
                    void* comp = world.GetComponentRaw((*capturedEntities)[i], typeId);
                    if (comp != nullptr)
                        *reinterpret_cast<T*>(static_cast<std::byte*>(comp) + fieldOffset) = newValue;
                }
            });
    }
} // namespace forge
