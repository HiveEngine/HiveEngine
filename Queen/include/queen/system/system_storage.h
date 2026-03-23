#pragma once

#include <comb/allocator_concepts.h>

#include <wax/containers/vector.h>

#include <queen/system/system.h>
#include <queen/system/system_builder.h>

namespace queen
{
    class World;

    /**
     * Storage and management for registered systems
     *
     * SystemStorage holds all registered systems and provides methods for
     * system registration, lookup, and execution. Each system is stored
     * with its descriptor containing name, access pattern, and executor.
     *
     * Memory layout:
     * ┌─────────────────────────────────────────────────────────────────┐
     * │ systems_: Vector<SystemDescriptor>                              │
     * └─────────────────────────────────────────────────────────────────┘
     *
     * Performance characteristics:
     * - RegisterSystem: O(1) amortized
     * - GetSystem(id): O(1) array access
     * - GetSystem(name): O(n) linear search
     * - RunSystem: O(1) + system execution time
     * - RunAll: O(n) systems
     *
     * Limitations:
     * - Systems are stored in registration order
     * - No automatic parallel execution (requires scheduler)
     *
     * Example:
     * @code
     *   SystemStorage storage{alloc};
     *
     *   SystemId id = storage.RegisterSystem<Read<Position>>(world, "Movement");
     *   storage.GetSystem(id)->each([](const Position& pos) { ... });
     *
     *   storage.RunSystem(world, id);
     *   storage.RunAll(world);
     * @endcode
     */
    template <comb::Allocator Allocator> class SystemStorage
    {
    public:
        explicit SystemStorage(Allocator& allocator)
            : m_allocator{&allocator}
            , m_systems{allocator}
        {
        }

        ~SystemStorage() = default;

        SystemStorage(const SystemStorage&) = delete;
        SystemStorage& operator=(const SystemStorage&) = delete;
        SystemStorage(SystemStorage&&) = default;
        SystemStorage& operator=(SystemStorage&&) = default;

        /**
         * Register a new system with query terms
         *
         * @tparam Terms Query term types
         * @param world The world this system operates on
         * @param name System name (for debugging)
         * @return SystemBuilder for further configuration
         */
        template <typename... Terms> SystemBuilder<Allocator, Terms...> Register(World& world, const char* name)
        {
            SystemId id{static_cast<uint32_t>(m_systems.Size())};

            m_systems.EmplaceBack(*m_allocator, id, name);

            return SystemBuilder<Allocator, Terms...>{world, *m_allocator, *this, &m_systems[m_systems.Size() - 1]};
        }

        /**
         * Register a simple system with a direct callback
         *
         * For testing and simple use cases where no query is needed.
         *
         * @tparam F Callable type with signature void(World<Allocator>&)
         * @param name System name
         * @param func Function to execute
         * @param access Access descriptor for dependency resolution (moved)
         * @return System ID
         */
        template <typename F> SystemId Register(const char* name, F&& func, AccessDescriptor<Allocator>&& access)
        {
            SystemId id{static_cast<uint32_t>(m_systems.Size())};

            m_systems.EmplaceBack(*m_allocator, id, name);

            auto& desc = m_systems[m_systems.Size() - 1];
            desc.Access() = std::move(access);

            using FuncType = std::decay_t<F>;
            void* userData = m_allocator->Allocate(sizeof(FuncType), alignof(FuncType));
            new (userData) FuncType{std::forward<F>(func)};

            desc.SetExecutor(
                [](World& world, void* data) {
                    auto* fn = static_cast<FuncType*>(data);
                    (*fn)(world);
                },
                userData, [](void* data) { static_cast<FuncType*>(data)->~FuncType(); });

            return id;
        }

        /**
         * Get a system by ID
         *
         * @param id System identifier
         * @return Pointer to system descriptor, or nullptr if invalid
         */
        [[nodiscard]] SystemDescriptor<Allocator>* GetSystem(SystemId id)
        {
            if (!id.IsValid() || id.Index() >= m_systems.Size())
            {
                return nullptr;
            }
            return &m_systems[id.Index()];
        }

        [[nodiscard]] const SystemDescriptor<Allocator>* GetSystem(SystemId id) const
        {
            if (!id.IsValid() || id.Index() >= m_systems.Size())
            {
                return nullptr;
            }
            return &m_systems[id.Index()];
        }

        /**
         * Get a system by index (for iteration)
         *
         * @param index Index in storage
         * @return Pointer to system descriptor, or nullptr if invalid
         */
        [[nodiscard]] SystemDescriptor<Allocator>* GetSystemByIndex(size_t index)
        {
            if (index >= m_systems.Size())
            {
                return nullptr;
            }
            return &m_systems[index];
        }

        [[nodiscard]] const SystemDescriptor<Allocator>* GetSystemByIndex(size_t index) const
        {
            if (index >= m_systems.Size())
            {
                return nullptr;
            }
            return &m_systems[index];
        }

        /**
         * Get a system by name
         *
         * @param name System name
         * @return Pointer to system descriptor, or nullptr if not found
         */
        [[nodiscard]] SystemDescriptor<Allocator>* GetSystemByName(const char* name)
        {
            for (size_t i = 0; i < m_systems.Size(); ++i)
            {
                if (std::strcmp(m_systems[i].Name(), name) == 0)
                {
                    return &m_systems[i];
                }
            }
            return nullptr;
        }

        /**
         * Run a specific system
         *
         * @param world The world to run the system on
         * @param id System to run
         */
        void RunSystem(World& world, SystemId id, Tick currentTick)
        {
            SystemDescriptor<Allocator>* system = GetSystem(id);
            if (system != nullptr)
            {
                system->Execute(world, currentTick);
            }
        }

        void RunAll(World& world, Tick currentTick)
        {
            for (size_t i = 0; i < m_systems.Size(); ++i)
            {
                m_systems[i].Execute(world, currentTick);
            }
        }

        /**
         * Get the number of registered systems
         */
        [[nodiscard]] size_t SystemCount() const noexcept
        {
            return m_systems.Size();
        }

        /**
         * Check if any systems are registered
         */
        [[nodiscard]] bool IsEmpty() const noexcept
        {
            return m_systems.IsEmpty();
        }

        /**
         * Enable or disable a system
         */
        void SetSystemEnabled(SystemId id, bool enabled)
        {
            SystemDescriptor<Allocator>* system = GetSystem(id);
            if (system != nullptr)
            {
                system->SetEnabled(enabled);
            }
        }

        /**
         * Check if a system is enabled
         */
        [[nodiscard]] bool IsSystemEnabled(SystemId id) const
        {
            const SystemDescriptor<Allocator>* system = GetSystem(id);
            if (system != nullptr)
            {
                return system->IsEnabled();
            }
            return false;
        }

        void RemoveSystem(SystemId id)
        {
            SystemDescriptor<Allocator>* system = GetSystem(id);
            if (system != nullptr)
            {
                system->SetExecutor(nullptr, nullptr, nullptr);
                system->SetEnabled(false);
            }
        }

        bool RemoveSystemByName(const char* name)
        {
            for (size_t i = 0; i < m_systems.Size(); ++i)
            {
                if (std::strcmp(m_systems[i].Name(), name) == 0)
                {
                    m_systems[i].SetExecutor(nullptr, nullptr, nullptr);
                    m_systems[i].SetEnabled(false);
                    return true;
                }
            }
            return false;
        }

        void Clear()
        {
            m_systems.Clear();
        }

    private:
        Allocator* m_allocator;
        wax::Vector<SystemDescriptor<Allocator>> m_systems;
    };
} // namespace queen
