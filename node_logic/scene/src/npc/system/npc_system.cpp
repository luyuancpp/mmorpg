#include "npc_system.h"

#include "proto/logic/component/actor_comp.pb.h"
#include "proto/logic/event/actor_event.pb.h"
#include "proto/logic/event/npc_event.pb.h"
#include "thread_local/storage.h"

void NpcSystem::InitializeNpcComponents(entt::entity npc)
{
    tls.registry.emplace<BaseAttributesPbComponent>(npc);
    tls.registry.emplace<LevelPbComponent>(npc);
}

void NpcSystem::CreateNpc()
{
    auto npc = tls.registry.create();

    InitializeActorComponentsEvent initializeActorComponentsEvent;
    initializeActorComponentsEvent.set_actor_entity(entt::to_integral(npc));
    tls.dispatcher.trigger(initializeActorComponentsEvent);
    
    InitializeNpcComponentsEvent initializeNpcComponents;
    initializeNpcComponents.set_actor_entity(entt::to_integral(npc));
    tls.dispatcher.trigger(initializeNpcComponents);

    tls.registry.get<LevelPbComponent>(npc).set_level(1);
}
