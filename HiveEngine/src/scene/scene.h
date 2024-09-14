//
// Created by GuillaumeIsCoding on 9/4/2024
//
#pragma once
#include <ostream>
#include <entt/entt.hpp>
#include "components.h"
#include "unordered_map"

namespace hive
{
    class Entity;
    class UUID;

    class Scene {
    public:
        Scene() = default;
        ~Scene() = default;

        Entity createEntity(const std::string& name = std::string());
        Entity createEntityWithID(UUID id, const std::string& name = std::string());
        void destroyEntity(Entity entity);

        friend std::ostream& operator<<(std::ostream& os, const Scene& scene);

        entt::registry& getRegistry();
    private:
        friend class Entity;
        entt::registry registry_;
        std::unordered_map<UUID, Entity> entities_;
    };
}