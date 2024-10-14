#include "npc_util.h"

#include "component/actor_comp.pb.h"
#include "event/player_event.pb.h"
#include "thread_local/storage.h"

void NpcUtil::InitializeNpcComponents(entt::entity entity)
{
    tls.registry.emplace<BaseAttributesPBComponent>(entity);
}

void NpcUtil::CreateNpc()
{
    auto npc = tls.registry.create();
    
    InitializePlayerComponentsEvent initializePlayerComponents;
    initializePlayerComponents.set_entity(entt::to_integral(npc));
    tls.dispatcher.trigger(initializePlayerComponents);
}
