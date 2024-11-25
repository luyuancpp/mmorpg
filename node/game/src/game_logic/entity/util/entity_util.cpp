#include "entity_util.h"

#include "proto/logic/component/npc_comp.pb.h"
#include "proto/logic/component/player_comp.pb.h"
#include "thread_local/storage.h"

bool EntityUtil::IsPlayer(const entt::entity entityId)
{
    return  tls.registry.any_of<Player>(entityId);
}

bool EntityUtil::IsNotPlayer(const entt::entity entityId)
{
    return  !tls.registry.any_of<Player>(entityId);
}

bool EntityUtil::IsNpc(const entt::entity entityId)
{
    return  tls.registry.any_of<Npc>(entityId);
}

bool EntityUtil::IsNotNpc(const entt::entity entityId)
{
    return  !tls.registry.any_of<Npc>(entityId);
}

