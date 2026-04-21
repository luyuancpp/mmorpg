#include "npc_event_handler.h"
#include "thread_context/ecs_context.h"

///<<< BEGIN WRITING YOUR CODE
#include <muduo/base/Logging.h>
///<<< END WRITING YOUR CODE
void NpcEventHandler::Register()
{
    tlsEcs.dispatcher.sink<InitializeNpcCompsEvent>().connect<&NpcEventHandler::InitializeNpcCompsEventHandler>();
}

void NpcEventHandler::UnRegister()
{
    tlsEcs.dispatcher.sink<InitializeNpcCompsEvent>().disconnect<&NpcEventHandler::InitializeNpcCompsEventHandler>();
}
void NpcEventHandler::InitializeNpcCompsEventHandler(const InitializeNpcCompsEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
	auto npc = entt::to_entity(event.actor_entity());

	if (!tlsEcs.actorRegistry.valid(npc))
	{
		LOG_ERROR << "Npc Not Found :" << event.actor_entity();
		return;
	}

///<<< END WRITING YOUR CODE
}
