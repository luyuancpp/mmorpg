#include "npc_event_handler.h"
#include "proto/common/event/npc_event.pb.h"
#include "threading/dispatcher_manager.h"

///<<< BEGIN WRITING YOUR CODE
#include "npc/system/npc.h"
#include <threading/registry_manager.h>
#include <muduo/base/Logging.h>
///<<< END WRITING YOUR CODE


void NpcEventHandler::Register()
{
    dispatcher.sink<InitializeNpcComponentsEvent>().connect<&NpcEventHandler::InitializeNpcComponentsEventHandler>();
}

void NpcEventHandler::UnRegister()
{
    dispatcher.sink<InitializeNpcComponentsEvent>().disconnect<&NpcEventHandler::InitializeNpcComponentsEventHandler>();
}
void NpcEventHandler::InitializeNpcComponentsEventHandler(const InitializeNpcComponentsEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
	auto npc = entt::to_entity(event.actor_entity());

	if (!tlsRegistryManager.actorRegistry.valid(npc))
	{
		LOG_ERROR << "Npc Not Found :" << event.actor_entity();
		return;
	}


///<<< END WRITING YOUR CODE
}
