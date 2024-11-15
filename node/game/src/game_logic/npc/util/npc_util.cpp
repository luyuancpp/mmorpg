#include "npc_util.h"

#include "component/actor_comp.pb.h"
#include "event/npc_event.pb.h"
#include "thread_local/storage.h"

void NpcUtil::InitializeNpcComponents(entt::entity npc)
{
    tls.registry.emplace<BaseAttributesPbComponent>(npc);
    tls.registry.emplace<LevelPbComponent>(npc);
}

void NpcUtil::CreateNpc()
{
    auto npc = tls.registry.create();
    
    InitializeNpcComponentsEvent initializeNpcComponents;
    initializeNpcComponents.set_actor_entity(entt::to_integral(npc));
    tls.dispatcher.trigger(initializeNpcComponents);

    tls.registry.get<LevelPbComponent>(npc).set_level(1);
}
