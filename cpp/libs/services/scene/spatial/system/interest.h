#pragma once

#include <entt/src/entt/entity/entity.hpp>

class InterestSystem {
public:
    static void AddAoiEntity(const entt::entity watcher, const entt::entity target);

    static void RemoveAoiEntity(const entt::entity watcher, const entt::entity target);
};

