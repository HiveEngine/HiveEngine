#pragma once

#include <queen/core/type_id.h>
#include <queen/core/entity.h>
#include <queen/core/component_info.h>
#include <comb/allocator_concepts.h>
#include <wax/containers/vector.h>
#include <hive/core/assert.h>

namespace queen
{
    class World;

    /**
     * Type of deferred command to execute on the World
     */
    enum class CommandType : uint8_t
    {
        Spawn,           // Create a new entity
        Despawn,         // Destroy an entity
        AddComponent,    // Add a component to an entity
        RemoveComponent, // Remove a component from an entity
        SetComponent,    // Set/update a component on an entity
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
    template<comb::Allocator Allocator>
    class CommandBuffer;

    template<comb::Allocator Allocator>
    class SpawnCommandBuilder;

    namespace detail
    {
        static constexpr size_t kCommandBlockSize = 4096;

        struct CommandDataBlock
        {
            alignas(alignof(std::max_align_t)) uint8_t data[kCommandBlockSize];
            size_t used = 0;
            CommandDataBlock* next = nullptr;
        };

        struct Command
        {
            CommandType type;
            Entity entity;          // Target entity or pending entity index
            TypeId component_type;  // Component type for add/remove/set
            void* data;             // Component data (for add/set)
            size_t data_size;       // Size of component data
            ComponentMeta meta;     // Lifecycle info for component
        };
    }

    /**
     * Builder for spawning entities with components via CommandBuffer
     */
    template<comb::Allocator Allocator>
    class SpawnCommandBuilder
    {
    public:
        SpawnCommandBuilder(CommandBuffer<Allocator>& buffer, uint32_t spawn_index)
            : buffer_{&buffer}
            , spawn_index_{spawn_index}
        {
        }

        template<typename T>
        SpawnCommandBuilder& With(T&& component);

        uint32_t GetSpawnIndex() const noexcept { return spawn_index_; }

    private:
        CommandBuffer<Allocator>* buffer_;
        uint32_t spawn_index_;
    };

    template<comb::Allocator Allocator>
    class CommandBuffer
    {
    public:
        explicit CommandBuffer(Allocator& allocator)
            : allocator_{&allocator}
            , commands_{allocator}
            , spawned_entities_{allocator}
            , head_block_{nullptr}
            , current_block_{nullptr}
            , spawn_count_{0}
        {
        }

        ~CommandBuffer()
        {
            ClearBlocks();
        }

        CommandBuffer(const CommandBuffer&) = delete;
        CommandBuffer& operator=(const CommandBuffer&) = delete;

        CommandBuffer(CommandBuffer&& other) noexcept
            : allocator_{other.allocator_}
            , commands_{static_cast<wax::Vector<detail::Command, Allocator>&&>(other.commands_)}
            , spawned_entities_{static_cast<wax::Vector<Entity, Allocator>&&>(other.spawned_entities_)}
            , head_block_{other.head_block_}
            , current_block_{other.current_block_}
            , spawn_count_{other.spawn_count_}
        {
            other.head_block_ = nullptr;
            other.current_block_ = nullptr;
            other.spawn_count_ = 0;
        }

        CommandBuffer& operator=(CommandBuffer&& other) noexcept
        {
            if (this != &other)
            {
                ClearBlocks();
                allocator_ = other.allocator_;
                commands_ = static_cast<wax::Vector<detail::Command, Allocator>&&>(other.commands_);
                spawned_entities_ = static_cast<wax::Vector<Entity, Allocator>&&>(other.spawned_entities_);
                head_block_ = other.head_block_;
                current_block_ = other.current_block_;
                spawn_count_ = other.spawn_count_;
                other.head_block_ = nullptr;
                other.current_block_ = nullptr;
                other.spawn_count_ = 0;
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
        [[nodiscard]] SpawnCommandBuilder<Allocator> Spawn()
        {
            uint32_t spawn_index = spawn_count_++;

            detail::Command cmd{};
            cmd.type = CommandType::Spawn;
            cmd.entity = Entity{spawn_index, 0, Entity::Flags::kPendingDelete};
            cmd.component_type = 0;
            cmd.data = nullptr;
            cmd.data_size = 0;

            commands_.PushBack(cmd);

            return SpawnCommandBuilder<Allocator>{*this, spawn_index};
        }

        /**
         * Queue a despawn command for an entity
         *
         * @param entity Entity to despawn (must be alive at Flush time)
         */
        void Despawn(Entity entity)
        {
            detail::Command cmd{};
            cmd.type = CommandType::Despawn;
            cmd.entity = entity;
            cmd.component_type = 0;
            cmd.data = nullptr;
            cmd.data_size = 0;

            commands_.PushBack(cmd);
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
        template<typename T>
        void Add(Entity entity, T&& component)
        {
            using DecayedT = std::decay_t<T>;

            void* data = AllocateData(sizeof(DecayedT), alignof(DecayedT));
            new (data) DecayedT{std::forward<T>(component)};

            detail::Command cmd{};
            cmd.type = CommandType::AddComponent;
            cmd.entity = entity;
            cmd.component_type = TypeIdOf<DecayedT>();
            cmd.data = data;
            cmd.data_size = sizeof(DecayedT);
            cmd.meta = ComponentMeta::Of<DecayedT>();

            commands_.PushBack(cmd);
        }

        /**
         * Queue a remove component command
         *
         * @tparam T Component type to remove
         * @param entity Target entity
         */
        template<typename T>
        void Remove(Entity entity)
        {
            detail::Command cmd{};
            cmd.type = CommandType::RemoveComponent;
            cmd.entity = entity;
            cmd.component_type = TypeIdOf<T>();
            cmd.data = nullptr;
            cmd.data_size = 0;

            commands_.PushBack(cmd);
        }

        /**
         * Queue a set component command (add or update)
         *
         * @tparam T Component type
         * @param entity Target entity
         * @param component Component value to set
         */
        template<typename T>
        void Set(Entity entity, T&& component)
        {
            using DecayedT = std::decay_t<T>;

            void* data = AllocateData(sizeof(DecayedT), alignof(DecayedT));
            new (data) DecayedT{std::forward<T>(component)};

            detail::Command cmd{};
            cmd.type = CommandType::SetComponent;
            cmd.entity = entity;
            cmd.component_type = TypeIdOf<DecayedT>();
            cmd.data = data;
            cmd.data_size = sizeof(DecayedT);
            cmd.meta = ComponentMeta::Of<DecayedT>();

            commands_.PushBack(cmd);
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
        void Clear()
        {
            for (size_t i = 0; i < commands_.Size(); ++i)
            {
                const detail::Command& cmd = commands_[i];
                if (cmd.data != nullptr && cmd.meta.destruct != nullptr)
                {
                    cmd.meta.destruct(cmd.data);
                }
            }

            commands_.Clear();
            spawned_entities_.Clear();
            spawn_count_ = 0;

            ClearBlocks();
        }

        /**
         * Get the number of queued commands
         */
        [[nodiscard]] size_t CommandCount() const noexcept
        {
            return commands_.Size();
        }

        /**
         * Check if command buffer is empty
         */
        [[nodiscard]] bool IsEmpty() const noexcept
        {
            return commands_.IsEmpty();
        }

        /**
         * Get a spawned entity by its spawn index
         *
         * Only valid after Flush() has been called.
         *
         * @param spawn_index Index returned by SpawnCommandBuilder
         * @return The real entity if spawned, Entity::Invalid() otherwise
         */
        [[nodiscard]] Entity GetSpawnedEntity(uint32_t spawn_index) const noexcept
        {
            if (spawn_index < spawned_entities_.Size())
            {
                return spawned_entities_[spawn_index];
            }
            return Entity::Invalid();
        }

    private:
        friend class SpawnCommandBuilder<Allocator>;

        void AddComponentToSpawn(uint32_t spawn_index, const ComponentMeta& meta, void* data)
        {
            Entity pending{spawn_index, 0, Entity::Flags::kPendingDelete};

            detail::Command cmd{};
            cmd.type = CommandType::AddComponent;
            cmd.entity = pending;
            cmd.component_type = meta.type_id;
            cmd.data = data;
            cmd.data_size = meta.size;
            cmd.meta = meta;

            commands_.PushBack(cmd);
        }

        void* AllocateData(size_t size, size_t alignment)
        {
            if (current_block_ == nullptr)
            {
                AllocateNewBlock();
            }

            size_t aligned_offset = (current_block_->used + alignment - 1) & ~(alignment - 1);

            if (aligned_offset + size > detail::kCommandBlockSize)
            {
                AllocateNewBlock();
                aligned_offset = 0;
            }

            void* ptr = current_block_->data + aligned_offset;
            current_block_->used = aligned_offset + size;
            return ptr;
        }

        void AllocateNewBlock()
        {
            void* memory = allocator_->Allocate(sizeof(detail::CommandDataBlock), alignof(detail::CommandDataBlock));
            hive::Assert(memory != nullptr, "Failed to allocate command data block");

            auto* block = new (memory) detail::CommandDataBlock{};

            if (current_block_ != nullptr)
            {
                current_block_->next = block;
            }
            else
            {
                head_block_ = block;
            }

            current_block_ = block;
        }

        void ClearBlocks()
        {
            detail::CommandDataBlock* block = head_block_;
            while (block != nullptr)
            {
                detail::CommandDataBlock* next = block->next;
                allocator_->Deallocate(block);
                block = next;
            }

            head_block_ = nullptr;
            current_block_ = nullptr;
        }

        bool IsPendingEntity(Entity entity) const noexcept
        {
            return entity.HasFlag(Entity::Flags::kPendingDelete);
        }

        Entity ResolveEntity(Entity entity) const noexcept
        {
            if (IsPendingEntity(entity))
            {
                uint32_t spawn_index = entity.Index();
                if (spawn_index < spawned_entities_.Size())
                {
                    return spawned_entities_[spawn_index];
                }
                return Entity::Invalid();
            }
            return entity;
        }

        Allocator* allocator_;
        wax::Vector<detail::Command, Allocator> commands_;
        wax::Vector<Entity, Allocator> spawned_entities_;
        detail::CommandDataBlock* head_block_;
        detail::CommandDataBlock* current_block_;
        uint32_t spawn_count_;
    };

    template<comb::Allocator Allocator>
    template<typename T>
    SpawnCommandBuilder<Allocator>& SpawnCommandBuilder<Allocator>::With(T&& component)
    {
        using DecayedT = std::decay_t<T>;

        void* data = buffer_->AllocateData(sizeof(DecayedT), alignof(DecayedT));
        new (data) DecayedT{std::forward<T>(component)};

        buffer_->AddComponentToSpawn(spawn_index_, ComponentMeta::Of<DecayedT>(), data);

        return *this;
    }
}

// Note: CommandBuffer::Flush implementation is in world.h to avoid circular dependency
