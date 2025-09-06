#include "entity.h"

#include "proto/logic/component/npc_comp.pb.h"
#include "proto/logic/component/player_comp.pb.h"

#include <threading/registry_manager.h>

bool EntitySystem::IsPlayer(const entt::entity entityId)
{
    return  tlsRegistryManager.actorRegistry.any_of<Player>(entityId);
}

bool EntitySystem::IsNotPlayer(const entt::entity entityId)
{
    return  !tlsRegistryManager.actorRegistry.any_of<Player>(entityId);
}

bool EntitySystem::IsNpc(const entt::entity entityId)
{
    return  tlsRegistryManager.actorRegistry.any_of<Npc>(entityId);
}

bool EntitySystem::IsNotNpc(const entt::entity entityId)
{
    return  !tlsRegistryManager.actorRegistry.any_of<Npc>(entityId);
}

