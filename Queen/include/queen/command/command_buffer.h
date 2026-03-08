#pragma once

#include <hive/core/assert.h>

#include <comb/allocator_concepts.h>

#include <wax/containers/vector.h>

#include <queen/core/component_info.h>
#include <queen/core/entity.h>
#include <queen/core/type_id.h>

namespace queen
{
    class World;

    /**
     * Type of deferred command to execute on the World
     */
    enum class CommandType : uint8_t
    {
        SPAWN,            // Create a new entity
        DESPAWN,          // Destroy an entity
        ADD_COMPONENT,    // Add a component to an entity
        REMOVE_COMPONENT, // Remove a component from an entity
        SET_COMPONENT,    // Set/update a component on an entity
    };

    /**
     * Deferred command buffer for safe structural mutations during iteration
     *
     * CommandBuffer allows deferred modification of the World, enabling safe
     * spawn/despawn/add/remove operations during query iteration. Commands are
     * queued and applied atomically when Flush() is called.
     *
     * Memory layout:
     * ┌────────────────────────────────────────────────────────────────────┐
     * │ commands_: Vector<Command> (command descriptors)                   │
     * │ data_blocks_: Linked list of 4KB blocks for component data         │
     * │ pending_entities_: Entities pre-allocated for Spawn commands       │
     * └────────────────────────────────────────────────────────────────────┘
     *
     * Data block structure:
     * ┌────────────────────────────────────────────────────────────────────┐
     * │ Block 0 (4KB)     │ Block 1 (4KB)     │ Block N (4KB)              │
     * │ [Component data]  │ [Component data]  │ [Component data]           │
     * │ [Component data]  │ [...]             │ [...]                      │
     * └────────────────────────────────────────────────────────────────────┘
     *
     * Performance characteristics:
     * - Spawn/Despawn/Add/Remove/Set: O(1) (append command)
     * - Flush: O(n) where n = total commands
     * - Memory: Block-based allocation reduces fragmentation
     *
     * Thread safety:
     * - NOT thread-safe. Use per-thread CommandBuffers for parallel systems.
     *
     * Limitations:
     * - Entity from Spawn() is a placeholder until Flush()
     * - Cannot query spawned entities before Flush()
     * - Commands applied in insertion order
     *
     * Use cases:
     * - Spawning/despawning during Each() iteration
     * - Deferred component modification
     * - Batch structural changes for performance
     * - System command accumulation before sync point
     *
     * Example:
     * @code
     *   comb::LinearAllocator alloc{1_MB};
     *   queen::World world{alloc};
     *   queen::CommandBuffer cmd{alloc};
     *
     *   // During iteration - cannot modify World directly
     *   world.Query<Read<Health>>().EachWithEntity([&](Entity e, const Health& hp) {
     *       if (hp.value <= 0) {
     *           cmd.Despawn(e);  // Deferred
     *       }
     *   });
     *
     *   // Apply all deferred commands
     *   cmd.Flush(world);
     *
     *   // Spawn with components
     *   Entity pending = cmd.Spawn()
     *       .With(Position{0, 0, 0})
     *       .With(Velocity{1, 0, 0})
     *       .Build();
     *
     *   cmd.Flush(world);  // Now entity exists in World
     * @endcode
     */
    template <comb::Allocator Allocator> class CommandBuffer;

    template <comb::Allocator Allocator> class SpawnCommandBuilder;

    namespace detail
    {
        static constexpr size_t kCommandBlockSize = 4096;

        struct CommandDataBlock
        {
            alignas(alignof(std::max_align_t)) uint8_t m_data[kCommandBlockSize];
            size_t m_used = 0;
            CommandDataBlock* m_next = nullptr;
        };

        struct Command
        {
            CommandType m_type;
            Entity m_entity;        // Target entity or pending entity index
            TypeId m_componentType; // Component type for add/remove/set
            void* m_data;           // Component data (for add/set)
            size_t m_dataSize;      // Size of component data
            ComponentMeta m_meta;   // Lifecycle info for component
        };
    } // namespace detail

    /**
     * Builder for spawning entities with components via CommandBuffer
     */
    template <comb::Allocator Allocator> class SpawnCommandBuilder
    {
    public:
        SpawnCommandBuilder(CommandBuffer<Allocator>& buffer, uint32_t spawnIndex)
            : m_buffer{&buffer}
            , m_spawnIndex{spawnIndex} {}

        template <typename T> SpawnCommandBuilder& With(T&& component);

        uint32_t GetSpawnIndex() const noexcept { return m_spawnIndex; }

    private:
        CommandBuffer<Allocator>* m_buffer;
        uint32_t m_spawnIndex;
    };

    template <comb::Allocator Allocator> class CommandBuffer
    {
    public:
        explicit CommandBuffer(Allocator& allocator)
            : m_allocator{&allocator}
            , m_commands{allocator}
            , m_spawnedEntities{allocator}
            , m_headBlock{nullptr}
            , m_currentBlock{nullptr}
            , m_spawnCount{0} {}

        ~CommandBuffer() { ClearBlocks(); }

        CommandBuffer(const CommandBuffer&) = delete;
        CommandBuffer& operator=(const CommandBuffer&) = delete;

        CommandBuffer(CommandBuffer&& other) noexcept
            : m_allocator{other.m_allocator}
            , m_commands{static_cast<wax::Vector<detail::Command>&&>(other.m_commands)}
            , m_spawnedEntities{static_cast<wax::Vector<Entity>&&>(other.m_spawnedEntities)}
            , m_headBlock{other.m_headBlock}
            , m_currentBlock{other.m_currentBlock}
            , m_spawnCount{other.m_spawnCount} {
            other.m_headBlock = nullptr;
            other.m_currentBlock = nullptr;
            other.m_spawnCount = 0;
        }

        CommandBuffer& operator=(CommandBuffer&& other) noexcept {
            if (this != &other)
            {
                ClearBlocks();
                m_allocator = other.m_allocator;
                m_commands = static_cast<wax::Vector<detail::Command>&&>(other.m_commands);
                m_spawnedEntities = static_cast<wax::Vector<Entity>&&>(other.m_spawnedEntities);
                m_headBlock = other.m_headBlock;
                m_currentBlock = other.m_currentBlock;
                m_spawnCount = other.m_spawnCount;
                other.m_headBlock = nullptr;
                other.m_currentBlock = nullptr;
                other.m_spawnCount = 0;
            }
            return *this;
        }

        /**
         * Queue a spawn command for a new entity
         *
         * Returns a builder to add components to the pending entity.
         * The entity will be created when Flush() is called.
         *
         * @return SpawnCommandBuilder for chaining component additions
         */
        [[nodiscard]] SpawnCommandBuilder<Allocator> Spawn() {
            uint32_t spawnIndex = m_spawnCount++;

            detail::Command cmd{};
            cmd.m_type = CommandType::SPAWN;
            cmd.m_entity = Entity{spawnIndex, 0, Entity::Flags::kPendingDelete};
            cmd.m_componentType = 0;
            cmd.m_data = nullptr;
            cmd.m_dataSize = 0;

            m_commands.PushBack(cmd);

            return SpawnCommandBuilder<Allocator>{*this, spawnIndex};
        }

        /**
         * Queue a despawn command for an entity
         *
         * @param entity Entity to despawn (must be alive at Flush time)
         */
        void Despawn(Entity entity) {
            detail::Command cmd{};
            cmd.m_type = CommandType::DESPAWN;
            cmd.m_entity = entity;
            cmd.m_componentType = 0;
            cmd.m_data = nullptr;
            cmd.m_dataSize = 0;

            m_commands.PushBack(cmd);
        }

        /**
         * Queue an add component command
         *
         * If the entity already has the component, this acts as Set.
         *
         * @tparam T Component type
         * @param entity Target entity
         * @param component Component value to add
         */
        template <typename T> void Add(Entity entity, T&& component) {
            using DecayedT = std::decay_t<T>;

            void* data = AllocateData(sizeof(DecayedT), alignof(DecayedT));
            new (data) DecayedT{std::forward<T>(component)};

            detail::Command cmd{};
            cmd.m_type = CommandType::ADD_COMPONENT;
            cmd.m_entity = entity;
            cmd.m_componentType = TypeIdOf<DecayedT>();
            cmd.m_data = data;
            cmd.m_dataSize = sizeof(DecayedT);
            cmd.m_meta = ComponentMeta::Of<DecayedT>();

            m_commands.PushBack(cmd);
        }

        /**
         * Queue a remove component command
         *
         * @tparam T Component type to remove
         * @param entity Target entity
         */
        template <typename T> void Remove(Entity entity) {
            detail::Command cmd{};
            cmd.m_type = CommandType::REMOVE_COMPONENT;
            cmd.m_entity = entity;
            cmd.m_componentType = TypeIdOf<T>();
            cmd.m_data = nullptr;
            cmd.m_dataSize = 0;

            m_commands.PushBack(cmd);
        }

        /**
         * Queue a set component command (add or update)
         *
         * @tparam T Component type
         * @param entity Target entity
         * @param component Component value to set
         */
        template <typename T> void Set(Entity entity, T&& component) {
            using DecayedT = std::decay_t<T>;

            void* data = AllocateData(sizeof(DecayedT), alignof(DecayedT));
            new (data) DecayedT{std::forward<T>(component)};

            detail::Command cmd{};
            cmd.m_type = CommandType::SET_COMPONENT;
            cmd.m_entity = entity;
            cmd.m_componentType = TypeIdOf<DecayedT>();
            cmd.m_data = data;
            cmd.m_dataSize = sizeof(DecayedT);
            cmd.m_meta = ComponentMeta::Of<DecayedT>();

            m_commands.PushBack(cmd);
        }

        /**
         * Apply all queued commands to the World
         *
         * Commands are applied in insertion order. After Flush(), the
         * CommandBuffer is cleared and ready for reuse.
         *
         * @param world Target World to apply commands to
         */
        void Flush(World& world);

        /**
         * Clear all queued commands without applying them
         */
        void Clear() {
            for (size_t i = 0; i < m_commands.Size(); ++i)
            {
                const detail::Command& cmd = m_commands[i];
                if (cmd.m_data != nullptr && cmd.m_meta.m_destruct != nullptr)
                {
                    cmd.m_meta.m_destruct(cmd.m_data);
                }
            }

            m_commands.Clear();
            m_spawnedEntities.Clear();
            m_spawnCount = 0;

            ClearBlocks();
        }

        /**
         * Get the number of queued commands
         */
        [[nodiscard]] size_t CommandCount() const noexcept { return m_commands.Size(); }

        /**
         * Check if command buffer is empty
         */
        [[nodiscard]] bool IsEmpty() const noexcept { return m_commands.IsEmpty(); }

        /**
         * Get a spawned entity by its spawn index
         *
         * Only valid after Flush() has been called.
         *
         * @param spawn_index Index returned by SpawnCommandBuilder
         * @return The real entity if spawned, Entity::Invalid() otherwise
         */
        [[nodiscard]] Entity GetSpawnedEntity(uint32_t spawnIndex) const noexcept {
            if (spawnIndex < m_spawnedEntities.Size())
            {
                return m_spawnedEntities[spawnIndex];
            }
            return Entity::Invalid();
        }

    private:
        friend class SpawnCommandBuilder<Allocator>;

        void AddComponentToSpawn(uint32_t spawnIndex, const ComponentMeta& meta, void* data) {
            Entity pending{spawnIndex, 0, Entity::Flags::kPendingDelete};

            detail::Command cmd{};
            cmd.m_type = CommandType::ADD_COMPONENT;
            cmd.m_entity = pending;
            cmd.m_componentType = meta.m_typeId;
            cmd.m_data = data;
            cmd.m_dataSize = meta.m_size;
            cmd.m_meta = meta;

            m_commands.PushBack(cmd);
        }

        void* AllocateData(size_t size, size_t alignment) {
            if (m_currentBlock == nullptr)
            {
                AllocateNewBlock();
            }

            size_t alignedOffset = (m_currentBlock->m_used + alignment - 1) & ~(alignment - 1);

            if (alignedOffset + size > detail::kCommandBlockSize)
            {
                AllocateNewBlock();
                alignedOffset = 0;
            }

            void* ptr = m_currentBlock->m_data + alignedOffset;
            m_currentBlock->m_used = alignedOffset + size;
            return ptr;
        }

        void AllocateNewBlock() {
            void* memory = m_allocator->Allocate(sizeof(detail::CommandDataBlock), alignof(detail::CommandDataBlock));
            hive::Assert(memory != nullptr, "Failed to allocate command data block");

            auto* block = new (memory) detail::CommandDataBlock{};

            if (m_currentBlock != nullptr)
            {
                m_currentBlock->m_next = block;
            }
            else
            {
                m_headBlock = block;
            }

            m_currentBlock = block;
        }

        void ClearBlocks() {
            detail::CommandDataBlock* block = m_headBlock;
            while (block != nullptr)
            {
                detail::CommandDataBlock* next = block->m_next;
                m_allocator->Deallocate(block);
                block = next;
            }

            m_headBlock = nullptr;
            m_currentBlock = nullptr;
        }

        bool IsPendingEntity(Entity entity) const noexcept { return entity.HasFlag(Entity::Flags::kPendingDelete); }

        Entity ResolveEntity(Entity entity) const noexcept {
            if (IsPendingEntity(entity))
            {
                uint32_t spawnIndex = entity.Index();
                if (spawnIndex < m_spawnedEntities.Size())
                {
                    return m_spawnedEntities[spawnIndex];
                }
                return Entity::Invalid();
            }
            return entity;
        }

        Allocator* m_allocator;
        wax::Vector<detail::Command> m_commands;
        wax::Vector<Entity> m_spawnedEntities;
        detail::CommandDataBlock* m_headBlock;
        detail::CommandDataBlock* m_currentBlock;
        uint32_t m_spawnCount;
    };

    template <comb::Allocator Allocator>
    template <typename T>
    SpawnCommandBuilder<Allocator>& SpawnCommandBuilder<Allocator>::With(T&& component) {
        using DecayedT = std::decay_t<T>;

        void* data = m_buffer->AllocateData(sizeof(DecayedT), alignof(DecayedT));
        new (data) DecayedT{std::forward<T>(component)};

        m_buffer->AddComponentToSpawn(m_spawnIndex, ComponentMeta::Of<DecayedT>(), data);

        return *this;
    }
} // namespace queen

// Note: CommandBuffer::Flush implementation is in world.h to avoid circular dependency
