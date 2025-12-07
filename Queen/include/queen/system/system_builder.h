#pragma once

#include <queen/system/system.h>
#include <queen/system/resource_param.h>
#include <queen/query/query_term.h>
#include <queen/query/query.h>
#include <queen/command/commands.h>
#include <comb/allocator_concepts.h>

namespace queen
{
    class World;

    template<comb::Allocator Allocator>
    class SystemStorage;

    /**
     * Builder for registering systems with the World
     *
     * SystemBuilder provides a fluent API for defining systems with their
     * queries, resource access, and execution callbacks. The builder
     * automatically extracts the access pattern from the query terms.
     *
     * Memory layout:
     * ┌─────────────────────────────────────────────────────────────────┐
     * │ world_: World* (reference to parent world)                      │
     * │ storage_: SystemStorage* (where system is registered)           │
     * │ descriptor_: SystemDescriptor* (system being built)             │
     * └─────────────────────────────────────────────────────────────────┘
     *
     * Performance characteristics:
     * - Building: O(n) where n = query terms
     * - each() registration: O(1)
     * - run() registration: O(1)
     *
     * Use cases:
     * - Entity iteration systems (query-based)
     * - Resource-only systems
     * - Exclusive world access systems
     *
     * Example:
     * @code
     *   // Entity system with query
     *   world.system<Read<Position>, Write<Velocity>>("Movement")
     *       .each([](const Position& pos, Velocity& vel) {
     *           vel.dx += pos.x * 0.1f;
     *       });
     *
     *   // Resource-only system
     *   world.system("UpdateTime")
     *       .with_resource<Time>()
     *       .run([](Time& time) {
     *           time.elapsed += 0.016f;
     *       });
     * @endcode
     */
    template<comb::Allocator Allocator, typename... Terms>
    class SystemBuilder
    {
    public:
        SystemBuilder(World& world, Allocator& allocator, SystemStorage<Allocator>& storage,
                     SystemDescriptor<Allocator>* descriptor)
            : world_{&world}
            , allocator_{&allocator}
            , storage_{&storage}
            , descriptor_{descriptor}
        {
            InitializeFromTerms();
        }

        /**
         * Set the system to run after another system
         */
        SystemBuilder& After(SystemId other)
        {
            // Store ordering constraint (to be used by scheduler)
            // For now, just a placeholder - will be implemented with scheduler
            (void)other;
            return *this;
        }

        /**
         * Set the system to run before another system
         */
        SystemBuilder& Before(SystemId other)
        {
            // Store ordering constraint (to be used by scheduler)
            (void)other;
            return *this;
        }

        /**
         * Mark system as exclusive (requires exclusive world access)
         */
        SystemBuilder& Exclusive()
        {
            descriptor_->SetExecutorMode(SystemExecutor::Exclusive);
            return *this;
        }

        /**
         * Add read access to a resource
         */
        template<typename T>
        SystemBuilder& WithResource()
        {
            descriptor_->Access().template AddResourceRead<T>();
            return *this;
        }

        /**
         * Add write access to a resource
         */
        template<typename T>
        SystemBuilder& WithResourceMut()
        {
            descriptor_->Access().template AddResourceWrite<T>();
            return *this;
        }

        /**
         * Register an entity iteration callback
         *
         * The callback receives component references matching the query terms.
         * It is called once for each entity matching the query.
         *
         * @tparam F Lambda type
         * @param func The callback function
         * @return SystemId for the registered system
         */
        template<typename F>
        SystemId Each(F&& func);  // Implementation in system_builder_impl.h

        /**
         * Register an entity iteration callback that also receives the Entity
         *
         * @tparam F Lambda type
         * @param func The callback function
         * @return SystemId for the registered system
         */
        template<typename F>
        SystemId EachWithEntity(F&& func);  // Implementation in system_builder_impl.h

        /**
         * Register an entity iteration callback with Commands access
         *
         * The callback receives the Entity, component references, and a reference
         * to the World's Commands collection for deferred mutations.
         *
         * @tparam F Lambda type (Entity, Components..., Commands<Allocator>&)
         * @param func The callback function
         * @return SystemId for the registered system
         *
         * Example:
         * @code
         *   world.System<Read<Health>>("DeathCheck")
         *       .EachWithCommands([](Entity e, const Health& hp, Commands<Allocator>& cmd) {
         *           if (hp.value <= 0) {
         *               cmd.Get().Despawn(e);
         *           }
         *       });
         * @endcode
         */
        template<typename F>
        SystemId EachWithCommands(F&& func);  // Implementation in system_builder_impl.h

        /**
         * Register a resource-only callback (no entity iteration)
         *
         * For systems that only access resources and don't iterate entities.
         * This requires explicit resource access declaration via
         * WithResource/WithResourceMut.
         *
         * @tparam F Lambda type
         * @param func The callback function
         * @return SystemId for the registered system
         */
        template<typename F>
        SystemId Run(F&& func)
        {
            using FuncType = std::decay_t<F>;

            void* user_data = allocator_->Allocate(sizeof(FuncType), alignof(FuncType));
            new (user_data) FuncType{std::forward<F>(func)};

            auto executor = [](World& world, void* data) {
                FuncType* fn = static_cast<FuncType*>(data);
                (*fn)(world);
            };

            auto destructor = [](void* data) {
                FuncType* fn = static_cast<FuncType*>(data);
                fn->~FuncType();
            };

            descriptor_->SetExecutor(executor, user_data, destructor);
            return descriptor_->Id();
        }

        /**
         * Register an entity iteration callback with a single Res<T> parameter
         *
         * The callback receives the Entity, component references, and a Res<T>.
         *
         * @tparam R Resource type (wrapped in Res<R>)
         * @tparam F Lambda type
         * @param func The callback function (Entity, Components..., Res<R>)
         * @return SystemId for the registered system
         *
         * Example:
         * @code
         *   world.System<Read<Position>>("Reader")
         *       .EachWithRes<Time>([](Entity e, const Position& pos, Res<Time> time) {
         *           // Use time->delta
         *       });
         * @endcode
         */
        template<typename R, typename F>
        SystemId EachWithRes(F&& func);  // Implementation in system_builder_impl.h

        /**
         * Register an entity iteration callback with a single ResMut<T> parameter
         *
         * The callback receives the Entity, component references, and a ResMut<T>.
         *
         * @tparam R Resource type (wrapped in ResMut<R>)
         * @tparam F Lambda type
         * @param func The callback function (Entity, Components..., ResMut<R>)
         * @return SystemId for the registered system
         *
         * Example:
         * @code
         *   world.System<Read<Position>>("Writer")
         *       .EachWithResMut<Time>([](Entity e, const Position& pos, ResMut<Time> time) {
         *           time->elapsed += time->delta;
         *       });
         * @endcode
         */
        template<typename R, typename F>
        SystemId EachWithResMut(F&& func);  // Implementation in system_builder_impl.h

        /**
         * Get the system ID (for ordering constraints)
         */
        [[nodiscard]] SystemId Id() const noexcept
        {
            return descriptor_->Id();
        }

    private:
        void InitializeFromTerms()
        {
            if constexpr (sizeof...(Terms) > 0)
            {
                (AddTermToAccess<Terms>(), ...);
                (AddTermToQuery<Terms>(), ...);
            }
        }

        template<typename T>
        void AddTermToAccess()
        {
            if constexpr (T::access == TermAccess::Read)
            {
                descriptor_->Access().AddComponentRead(T::type_id);
            }
            else if constexpr (T::access == TermAccess::Write)
            {
                descriptor_->Access().AddComponentWrite(T::type_id);
            }
        }

        template<typename T>
        void AddTermToQuery()
        {
            descriptor_->Query().AddTerm(T::ToTerm());
        }

        World* world_;
        Allocator* allocator_;
        SystemStorage<Allocator>* storage_;
        SystemDescriptor<Allocator>* descriptor_;
    };

    /**
     * Specialization for no query terms (resource-only systems)
     */
    template<comb::Allocator Allocator>
    class SystemBuilder<Allocator>
    {
    public:
        SystemBuilder(World& world, Allocator& allocator, SystemStorage<Allocator>& storage,
                     SystemDescriptor<Allocator>* descriptor)
            : world_{&world}
            , allocator_{&allocator}
            , storage_{&storage}
            , descriptor_{descriptor}
        {
        }

        SystemBuilder& Exclusive()
        {
            descriptor_->SetExecutorMode(SystemExecutor::Exclusive);
            return *this;
        }

        template<typename T>
        SystemBuilder& WithResource()
        {
            descriptor_->Access().template AddResourceRead<T>();
            return *this;
        }

        template<typename T>
        SystemBuilder& WithResourceMut()
        {
            descriptor_->Access().template AddResourceWrite<T>();
            return *this;
        }

        template<typename F>
        SystemId Run(F&& func)
        {
            using FuncType = std::decay_t<F>;

            void* user_data = allocator_->Allocate(sizeof(FuncType), alignof(FuncType));
            new (user_data) FuncType{std::forward<F>(func)};

            auto executor = [](World& world, void* data) {
                FuncType* fn = static_cast<FuncType*>(data);
                (*fn)(world);
            };

            auto destructor = [](void* data) {
                FuncType* fn = static_cast<FuncType*>(data);
                fn->~FuncType();
            };

            descriptor_->SetExecutor(executor, user_data, destructor);
            return descriptor_->Id();
        }

        /**
         * Register a resource-only callback with Res<T> parameter
         *
         * @tparam R Resource type
         * @tparam F Lambda type taking Res<R>
         * @param func The callback function
         * @return SystemId for the registered system
         *
         * Example:
         * @code
         *   world.System("ReadTime")
         *       .RunWithRes<Time>([](Res<Time> time) {
         *           // Use time->delta
         *       });
         * @endcode
         */
        template<typename R, typename F>
        SystemId RunWithRes(F&& func);  // Implementation in system_builder_impl.h

        /**
         * Register a resource-only callback with ResMut<T> parameter
         *
         * @tparam R Resource type
         * @tparam F Lambda type taking ResMut<R>
         * @param func The callback function
         * @return SystemId for the registered system
         *
         * Example:
         * @code
         *   world.System("UpdateTime")
         *       .RunWithResMut<Time>([](ResMut<Time> time) {
         *           time->elapsed += time->delta;
         *       });
         * @endcode
         */
        template<typename R, typename F>
        SystemId RunWithResMut(F&& func);  // Implementation in system_builder_impl.h

        [[nodiscard]] SystemId Id() const noexcept
        {
            return descriptor_->Id();
        }

    private:
        World* world_;
        Allocator* allocator_;
        SystemStorage<Allocator>* storage_;
        SystemDescriptor<Allocator>* descriptor_;
    };
}
