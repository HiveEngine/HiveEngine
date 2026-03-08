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

    template <comb::Allocator Allocator, typename... Terms>
    template <typename F>
    SystemId SystemBuilder<Allocator, Terms...>::Each(F&& func)
    {
        using FuncType = std::decay_t<F>;

        void* userData = m_allocator->Allocate(sizeof(FuncType), alignof(FuncType));
        new (userData) FuncType{std::forward<F>(func)};

        auto executor = [](World& world, void* data) {
            FuncType* fn = static_cast<FuncType*>(data);
            // Use locked version to protect Query lifetime (construction + destruction)
            world.template QueryEachLocked<Terms...>(*fn);
        };

        auto destructor = [](void* data) {
            FuncType* fn = static_cast<FuncType*>(data);
            fn->~FuncType();
        };

        m_descriptor->SetExecutor(executor, userData, destructor);
        return m_descriptor->Id();
    }

    template <comb::Allocator Allocator, typename... Terms>
    template <typename F>
    SystemId SystemBuilder<Allocator, Terms...>::EachWithEntity(F&& func)
    {
        using FuncType = std::decay_t<F>;

        void* userData = m_allocator->Allocate(sizeof(FuncType), alignof(FuncType));
        new (userData) FuncType{std::forward<F>(func)};

        auto executor = [](World& world, void* data) {
            FuncType* fn = static_cast<FuncType*>(data);
            // Use locked version to protect Query lifetime (construction + destruction)
            world.template QueryEachWithEntityLocked<Terms...>(*fn);
        };

        auto destructor = [](void* data) {
            FuncType* fn = static_cast<FuncType*>(data);
            fn->~FuncType();
        };

        m_descriptor->SetExecutor(executor, userData, destructor);
        return m_descriptor->Id();
    }

    template <comb::Allocator Allocator, typename... Terms>
    template <typename F>
    SystemId SystemBuilder<Allocator, Terms...>::EachWithCommands(F&& func)
    {
        using FuncType = std::decay_t<F>;

        void* userData = m_allocator->Allocate(sizeof(FuncType), alignof(FuncType));
        new (userData) FuncType{std::forward<F>(func)};

        auto executor = [](World& world, void* data) {
            FuncType* fn = static_cast<FuncType*>(data);
            auto& commands = world.GetCommands();

            // Use locked version to protect Query lifetime (construction + destruction)
            world.template QueryEach<Terms...>([fn, &commands](auto& query) {
                query.EachWithEntity([fn, &commands](Entity e, auto&&... components) {
                    (*fn)(e, std::forward<decltype(components)>(components)..., commands);
                });
            });
        };

        auto destructor = [](void* data) {
            FuncType* fn = static_cast<FuncType*>(data);
            fn->~FuncType();
        };

        m_descriptor->SetExecutor(executor, userData, destructor);
        return m_descriptor->Id();
    }

    template <comb::Allocator Allocator, typename... Terms>
    template <typename R, typename F>
    SystemId SystemBuilder<Allocator, Terms...>::EachWithRes(F&& func)
    {
        using FuncType = std::decay_t<F>;

        m_descriptor->Access().template AddResourceRead<R>();

        void* userData = m_allocator->Allocate(sizeof(FuncType), alignof(FuncType));
        new (userData) FuncType{std::forward<F>(func)};

        auto executor = [](World& world, void* data) {
            FuncType* fn = static_cast<FuncType*>(data);
            R* resPtr = world.Resource<R>();
            hive::Assert(resPtr != nullptr, "Resource not found for Res<T>");
            Res<R> res{resPtr};

            // Use locked version to protect Query lifetime (construction + destruction)
            world.template QueryEach<Terms...>([fn, res](auto& query) {
                query.EachWithEntity([fn, res](Entity e, auto&&... components) {
                    (*fn)(e, std::forward<decltype(components)>(components)..., res);
                });
            });
        };

        auto destructor = [](void* data) {
            FuncType* fn = static_cast<FuncType*>(data);
            fn->~FuncType();
        };

        m_descriptor->SetExecutor(executor, userData, destructor);
        return m_descriptor->Id();
    }

    template <comb::Allocator Allocator, typename... Terms>
    template <typename R, typename F>
    SystemId SystemBuilder<Allocator, Terms...>::EachWithResMut(F&& func)
    {
        using FuncType = std::decay_t<F>;

        m_descriptor->Access().template AddResourceWrite<R>();

        void* userData = m_allocator->Allocate(sizeof(FuncType), alignof(FuncType));
        new (userData) FuncType{std::forward<F>(func)};

        auto executor = [](World& world, void* data) {
            FuncType* fn = static_cast<FuncType*>(data);
            R* resPtr = world.Resource<R>();
            hive::Assert(resPtr != nullptr, "Resource not found for ResMut<T>");
            ResMut<R> res{resPtr};

            // Use locked version to protect Query lifetime (construction + destruction)
            world.template QueryEach<Terms...>([fn, res](auto& query) {
                query.EachWithEntity([fn, res](Entity e, auto&&... components) {
                    (*fn)(e, std::forward<decltype(components)>(components)..., res);
                });
            });
        };

        auto destructor = [](void* data) {
            FuncType* fn = static_cast<FuncType*>(data);
            fn->~FuncType();
        };

        m_descriptor->SetExecutor(executor, userData, destructor);
        return m_descriptor->Id();
    }

    // ================================
    // SystemBuilder<Allocator> (no terms) implementations
    // ================================

    template <comb::Allocator Allocator>
    template <typename R, typename F>
    SystemId SystemBuilder<Allocator>::RunWithRes(F&& func)
    {
        using FuncType = std::decay_t<F>;

        m_descriptor->Access().template AddResourceRead<R>();

        void* userData = m_allocator->Allocate(sizeof(FuncType), alignof(FuncType));
        new (userData) FuncType{std::forward<F>(func)};

        auto executor = [](World& world, void* data) {
            FuncType* fn = static_cast<FuncType*>(data);
            R* resPtr = world.Resource<R>();
            hive::Assert(resPtr != nullptr, "Resource not found for Res<T>");
            Res<R> res{resPtr};
            (*fn)(res);
        };

        auto destructor = [](void* data) {
            FuncType* fn = static_cast<FuncType*>(data);
            fn->~FuncType();
        };

        m_descriptor->SetExecutor(executor, userData, destructor);
        return m_descriptor->Id();
    }

    template <comb::Allocator Allocator>
    template <typename R, typename F>
    SystemId SystemBuilder<Allocator>::RunWithResMut(F&& func)
    {
        using FuncType = std::decay_t<F>;

        m_descriptor->Access().template AddResourceWrite<R>();

        void* userData = m_allocator->Allocate(sizeof(FuncType), alignof(FuncType));
        new (userData) FuncType{std::forward<F>(func)};

        auto executor = [](World& world, void* data) {
            FuncType* fn = static_cast<FuncType*>(data);
            R* resPtr = world.Resource<R>();
            hive::Assert(resPtr != nullptr, "Resource not found for ResMut<T>");
            ResMut<R> res{resPtr};
            (*fn)(res);
        };

        auto destructor = [](void* data) {
            FuncType* fn = static_cast<FuncType*>(data);
            fn->~FuncType();
        };

        m_descriptor->SetExecutor(executor, userData, destructor);
        return m_descriptor->Id();
    }
} // namespace queen
