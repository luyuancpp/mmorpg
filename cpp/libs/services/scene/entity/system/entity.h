#pragma once
#include <entt/src/entt/entity/entity.hpp>

class EntitySystem {
public:
    static bool IsPlayer(const entt::entity entityId);
    
    static bool IsNotPlayer(const entt::entity entityId);

    static bool IsNpc(const entt::entity entityId);
    
    static bool IsNotNpc(const entt::entity entityId);
};
