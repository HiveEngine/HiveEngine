#pragma once

/**
 * SystemBuilder method implementations
 *
 * This file contains implementations of SystemBuilder methods that access World members.
 * It must be included AFTER the World class is fully defined.
 */

namespace queen
{
    // ================================
    // SystemBuilder<Allocator, Terms...> implementations
    // ================================

    template<comb::Allocator Allocator, typename... Terms>
    template<typename F>
    SystemId SystemBuilder<Allocator, Terms...>::Each(F&& func)
    {
        using FuncType = std::decay_t<F>;

        void* user_data = allocator_->Allocate(sizeof(FuncType), alignof(FuncType));
        new (user_data) FuncType{std::forward<F>(func)};

        auto executor = [](World& world, void* data) {
            FuncType* fn = static_cast<FuncType*>(data);
            auto query = world.Query<Terms...>();
            query.Each(*fn);
        };

        auto destructor = [](void* data) {
            FuncType* fn = static_cast<FuncType*>(data);
            fn->~FuncType();
        };

        descriptor_->SetExecutor(executor, user_data, destructor);
        return descriptor_->Id();
    }

    template<comb::Allocator Allocator, typename... Terms>
    template<typename F>
    SystemId SystemBuilder<Allocator, Terms...>::EachWithEntity(F&& func)
    {
        using FuncType = std::decay_t<F>;

        void* user_data = allocator_->Allocate(sizeof(FuncType), alignof(FuncType));
        new (user_data) FuncType{std::forward<F>(func)};

        auto executor = [](World& world, void* data) {
            FuncType* fn = static_cast<FuncType*>(data);
            auto query = world.Query<Terms...>();
            query.EachWithEntity(*fn);
        };

        auto destructor = [](void* data) {
            FuncType* fn = static_cast<FuncType*>(data);
            fn->~FuncType();
        };

        descriptor_->SetExecutor(executor, user_data, destructor);
        return descriptor_->Id();
    }

    template<comb::Allocator Allocator, typename... Terms>
    template<typename F>
    SystemId SystemBuilder<Allocator, Terms...>::EachWithCommands(F&& func)
    {
        using FuncType = std::decay_t<F>;

        void* user_data = allocator_->Allocate(sizeof(FuncType), alignof(FuncType));
        new (user_data) FuncType{std::forward<F>(func)};

        auto executor = [](World& world, void* data) {
            FuncType* fn = static_cast<FuncType*>(data);
            auto query = world.Query<Terms...>();
            auto& commands = world.GetCommands();

            query.EachWithEntity([fn, &commands](Entity e, auto&&... components) {
                (*fn)(e, std::forward<decltype(components)>(components)..., commands);
            });
        };

        auto destructor = [](void* data) {
            FuncType* fn = static_cast<FuncType*>(data);
            fn->~FuncType();
        };

        descriptor_->SetExecutor(executor, user_data, destructor);
        return descriptor_->Id();
    }

    template<comb::Allocator Allocator, typename... Terms>
    template<typename R, typename F>
    SystemId SystemBuilder<Allocator, Terms...>::EachWithRes(F&& func)
    {
        using FuncType = std::decay_t<F>;

        descriptor_->Access().template AddResourceRead<R>();

        void* user_data = allocator_->Allocate(sizeof(FuncType), alignof(FuncType));
        new (user_data) FuncType{std::forward<F>(func)};

        auto executor = [](World& world, void* data) {
            FuncType* fn = static_cast<FuncType*>(data);
            auto query = world.Query<Terms...>();
            R* res_ptr = world.Resource<R>();
            hive::Assert(res_ptr != nullptr, "Resource not found for Res<T>");
            Res<R> res{res_ptr};

            query.EachWithEntity([fn, res](Entity e, auto&&... components) {
                (*fn)(e, std::forward<decltype(components)>(components)..., res);
            });
        };

        auto destructor = [](void* data) {
            FuncType* fn = static_cast<FuncType*>(data);
            fn->~FuncType();
        };

        descriptor_->SetExecutor(executor, user_data, destructor);
        return descriptor_->Id();
    }

    template<comb::Allocator Allocator, typename... Terms>
    template<typename R, typename F>
    SystemId SystemBuilder<Allocator, Terms...>::EachWithResMut(F&& func)
    {
        using FuncType = std::decay_t<F>;

        descriptor_->Access().template AddResourceWrite<R>();

        void* user_data = allocator_->Allocate(sizeof(FuncType), alignof(FuncType));
        new (user_data) FuncType{std::forward<F>(func)};

        auto executor = [](World& world, void* data) {
            FuncType* fn = static_cast<FuncType*>(data);
            auto query = world.Query<Terms...>();
            R* res_ptr = world.Resource<R>();
            hive::Assert(res_ptr != nullptr, "Resource not found for ResMut<T>");
            ResMut<R> res{res_ptr};

            query.EachWithEntity([fn, res](Entity e, auto&&... components) {
                (*fn)(e, std::forward<decltype(components)>(components)..., res);
            });
        };

        auto destructor = [](void* data) {
            FuncType* fn = static_cast<FuncType*>(data);
            fn->~FuncType();
        };

        descriptor_->SetExecutor(executor, user_data, destructor);
        return descriptor_->Id();
    }

    // ================================
    // SystemBuilder<Allocator> (no terms) implementations
    // ================================

    template<comb::Allocator Allocator>
    template<typename R, typename F>
    SystemId SystemBuilder<Allocator>::RunWithRes(F&& func)
    {
        using FuncType = std::decay_t<F>;

        descriptor_->Access().template AddResourceRead<R>();

        void* user_data = allocator_->Allocate(sizeof(FuncType), alignof(FuncType));
        new (user_data) FuncType{std::forward<F>(func)};

        auto executor = [](World& world, void* data) {
            FuncType* fn = static_cast<FuncType*>(data);
            R* res_ptr = world.Resource<R>();
            hive::Assert(res_ptr != nullptr, "Resource not found for Res<T>");
            Res<R> res{res_ptr};
            (*fn)(res);
        };

        auto destructor = [](void* data) {
            FuncType* fn = static_cast<FuncType*>(data);
            fn->~FuncType();
        };

        descriptor_->SetExecutor(executor, user_data, destructor);
        return descriptor_->Id();
    }

    template<comb::Allocator Allocator>
    template<typename R, typename F>
    SystemId SystemBuilder<Allocator>::RunWithResMut(F&& func)
    {
        using FuncType = std::decay_t<F>;

        descriptor_->Access().template AddResourceWrite<R>();

        void* user_data = allocator_->Allocate(sizeof(FuncType), alignof(FuncType));
        new (user_data) FuncType{std::forward<F>(func)};

        auto executor = [](World& world, void* data) {
            FuncType* fn = static_cast<FuncType*>(data);
            R* res_ptr = world.Resource<R>();
            hive::Assert(res_ptr != nullptr, "Resource not found for ResMut<T>");
            ResMut<R> res{res_ptr};
            (*fn)(res);
        };

        auto destructor = [](void* data) {
            FuncType* fn = static_cast<FuncType*>(data);
            fn->~FuncType();
        };

        descriptor_->SetExecutor(executor, user_data, destructor);
        return descriptor_->Id();
    }
}
