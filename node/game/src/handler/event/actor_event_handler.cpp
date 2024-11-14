#include "actor_event_handler.h"

#include "game_logic/actor/actionstate/constants/actor_state_constants.h"
#include "game_logic/mount/util/mount_util.h"
#include "logic/event/actor_event.pb.h"
#include "thread_local/storage.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
void ActorEventHandler::Register()
{
		tls.dispatcher.sink<InterruptCurrentStatePbEvent>().connect<&ActorEventHandler::InterruptCurrentStatePbEventHandler>();
}

void ActorEventHandler::UnRegister()
{
		tls.dispatcher.sink<InterruptCurrentStatePbEvent>().disconnect<&ActorEventHandler::InterruptCurrentStatePbEventHandler>();
}

void ActorEventHandler::InterruptCurrentStatePbEventHandler(const InterruptCurrentStatePbEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
	entt::entity actorEntity = entt::to_entity(event.actor_entity());
	if (event.actor_state() == kActorStateMounted)
	{
		MountUtil::InterruptAndUnmountActor(actorEntity);
	}
///<<< END WRITING YOUR CODE
}

