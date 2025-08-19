#include "npc_event_handler.h"
#include "proto/logic/event/npc_event.pb.h"
#include "thread_local/storage.h"

///<<< BEGIN WRITING YOUR CODE
#include "npc/system/npc_system.h"
///<<< END WRITING YOUR CODE


void NpcEventHandler::Register()
{
    tls.dispatcher.sink<InitializeNpcComponentsEvent>().connect<&NpcEventHandler::InitializeNpcComponentsEventHandler>();
}

void NpcEventHandler::UnRegister()
{
    tls.dispatcher.sink<InitializeNpcComponentsEvent>().disconnect<&NpcEventHandler::InitializeNpcComponentsEventHandler>();
}
void NpcEventHandler::InitializeNpcComponentsEventHandler(const InitializeNpcComponentsEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
    auto npc = entt::to_entity(event.actor_entity());

    if (!tls.actorRegistry.valid(npc))
    {
        LOG_ERROR << "Npc Not Found :" << event.actor_entity();
        return;
    }
    
    NpcSystem::InitializeNpcComponents(npc);
    
///<<< END WRITING YOUR CODE
}
