#pragma once

#include <queen/system/system.h>
#include <queen/system/system_builder.h>
#include <wax/containers/vector.h>
#include <comb/allocator_concepts.h>

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
    template<comb::Allocator Allocator>
    class SystemStorage
    {
    public:
        explicit SystemStorage(Allocator& allocator)
            : allocator_{&allocator}
            , systems_{allocator}
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
        template<typename... Terms>
        SystemBuilder<Allocator, Terms...> Register(World& world, const char* name)
        {
            SystemId id{static_cast<uint32_t>(systems_.Size())};

            systems_.EmplaceBack(*allocator_, id, name);

            return SystemBuilder<Allocator, Terms...>{world, *allocator_, *this, &systems_[systems_.Size() - 1]};
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
        template<typename F>
        SystemId Register(const char* name, F&& func, AccessDescriptor<Allocator>&& access)
        {
            SystemId id{static_cast<uint32_t>(systems_.Size())};

            systems_.EmplaceBack(*allocator_, id, name);

            auto& desc = systems_[systems_.Size() - 1];
            desc.Access() = std::move(access);

            using FuncType = std::decay_t<F>;
            void* user_data = allocator_->Allocate(sizeof(FuncType), alignof(FuncType));
            new (user_data) FuncType{std::forward<F>(func)};

            desc.SetExecutor(
                [](World& world, void* data) {
                    auto* fn = static_cast<FuncType*>(data);
                    (*fn)(world);
                },
                user_data,
                [](void* data) {
                    static_cast<FuncType*>(data)->~FuncType();
                }
            );

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
            if (!id.IsValid() || id.Index() >= systems_.Size())
            {
                return nullptr;
            }
            return &systems_[id.Index()];
        }

        [[nodiscard]] const SystemDescriptor<Allocator>* GetSystem(SystemId id) const
        {
            if (!id.IsValid() || id.Index() >= systems_.Size())
            {
                return nullptr;
            }
            return &systems_[id.Index()];
        }

        /**
         * Get a system by index (for iteration)
         *
         * @param index Index in storage
         * @return Pointer to system descriptor, or nullptr if invalid
         */
        [[nodiscard]] SystemDescriptor<Allocator>* GetSystemByIndex(size_t index)
        {
            if (index >= systems_.Size())
            {
                return nullptr;
            }
            return &systems_[index];
        }

        [[nodiscard]] const SystemDescriptor<Allocator>* GetSystemByIndex(size_t index) const
        {
            if (index >= systems_.Size())
            {
                return nullptr;
            }
            return &systems_[index];
        }

        /**
         * Get a system by name
         *
         * @param name System name
         * @return Pointer to system descriptor, or nullptr if not found
         */
        [[nodiscard]] SystemDescriptor<Allocator>* GetSystemByName(const char* name)
        {
            for (size_t i = 0; i < systems_.Size(); ++i)
            {
                if (std::strcmp(systems_[i].Name(), name) == 0)
                {
                    return &systems_[i];
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
        void RunSystem(World& world, SystemId id, Tick current_tick)
        {
            SystemDescriptor<Allocator>* system = GetSystem(id);
            if (system != nullptr)
            {
                system->Execute(world, current_tick);
            }
        }

        void RunAll(World& world, Tick current_tick)
        {
            for (size_t i = 0; i < systems_.Size(); ++i)
            {
                systems_[i].Execute(world, current_tick);
            }
        }

        /**
         * Get the number of registered systems
         */
        [[nodiscard]] size_t SystemCount() const noexcept
        {
            return systems_.Size();
        }

        /**
         * Check if any systems are registered
         */
        [[nodiscard]] bool IsEmpty() const noexcept
        {
            return systems_.IsEmpty();
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

    private:
        Allocator* allocator_;
        wax::Vector<SystemDescriptor<Allocator>, Allocator> systems_;
    };
}
