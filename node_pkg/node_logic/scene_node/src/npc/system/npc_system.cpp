#include "npc_system.h"

#include "proto/logic/component/actor_comp.pb.h"
#include "proto/logic/event/actor_event.pb.h"
#include "proto/logic/event/npc_event.pb.h"

#include <thread_local/registry_manager.h>
#include <thread_local/dispatcher_manager.h>

void NpcSystem::InitializeNpcComponents(entt::entity npc)
{
    tlsRegistryManager.actorRegistry.emplace<BaseAttributesPbComponent>(npc);
    tlsRegistryManager.actorRegistry.emplace<LevelPbComponent>(npc);
}

void NpcSystem::CreateNpc()
{
    auto npc = tlsRegistryManager.actorRegistry.create();

    InitializeActorComponentsEvent initializeActorComponentsEvent;
    initializeActorComponentsEvent.set_actor_entity(entt::to_integral(npc));
    dispatcher.trigger(initializeActorComponentsEvent);
    
    InitializeNpcComponentsEvent initializeNpcComponents;
    initializeNpcComponents.set_actor_entity(entt::to_integral(npc));
    dispatcher.trigger(initializeNpcComponents);

    tlsRegistryManager.actorRegistry.get<LevelPbComponent>(npc).set_level(1);
}
