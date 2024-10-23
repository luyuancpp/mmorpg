#pragma once
#include <entt/src/entt/entity/entity.hpp>

class ActorAttributeCalculatorUtil {
public:
    static void Initialize();

    static void InitializeActorComponents(entt::entity entity);

    static void UpdateActorState(entt::entity actorEntity);

    static void MarkAttributeForUpdate(entt::entity actorEntity, uint32_t attributeBit);
};
