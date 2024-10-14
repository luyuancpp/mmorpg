#include "npc_util.h"

#include "component/actor_comp.pb.h"
#include "event/npc_event.pb.h"
#include "thread_local/storage.h"

void NpcUtil::InitializeNpcComponents(entt::entity npc)
{
    tls.registry.emplace<BaseAttributesPBComponent>(npc);
    tls.registry.emplace<LevelComponent>(npc);
}

void NpcUtil::CreateNpc()
{
    auto npc = tls.registry.create();
    
    InitializeNpcComponentsEvent initializeNpcComponents;
    initializeNpcComponents.set_entity(entt::to_integral(npc));
    tls.dispatcher.trigger(initializeNpcComponents);

    tls.registry.get<LevelComponent>(npc).set_level(1);
}
