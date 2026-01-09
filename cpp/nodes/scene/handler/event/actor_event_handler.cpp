#include "actor_event_handler.h"
#include "proto/logic/event/actor_event.pb.h"
#include "threading/dispatcher_manager.h"

///<<< BEGIN WRITING YOUR CODE
#include "actor/action_state/constants/actor_state.h"
#include "actor/action_state/system/actor_action_state.h"
#include "actor/attribute/system/actor_attribute_calculator.h"
#include "actor/attribute/system/actor_state_attribute_sync.h"
#include "scene/combat_state/system/combat_state.h"
#include "scene/combat/buff/system/buff.h"
#include "scene/combat/skill/system/skill.h"
#include "mount/system/mount.h"
#include "scene/scene/system/interest.h"
///<<< END WRITING YOUR CODE


void ActorEventHandler::Register()
{
    dispatcher.sink<InitializeActorComponentsEvent>().connect<&ActorEventHandler::InitializeActorComponentsEventHandler>();
    dispatcher.sink<InterruptCurrentStatePbEvent>().connect<&ActorEventHandler::InterruptCurrentStatePbEventHandler>();
}

void ActorEventHandler::UnRegister()
{
    dispatcher.sink<InitializeActorComponentsEvent>().disconnect<&ActorEventHandler::InitializeActorComponentsEventHandler>();
    dispatcher.sink<InterruptCurrentStatePbEvent>().disconnect<&ActorEventHandler::InterruptCurrentStatePbEventHandler>();
}
void ActorEventHandler::InitializeActorComponentsEventHandler(const InitializeActorComponentsEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
	const entt::entity actorEntity = entt::to_entity(event.actor_entity());
///<<< END WRITING YOUR CODE
}
void ActorEventHandler::InterruptCurrentStatePbEventHandler(const InterruptCurrentStatePbEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
	const entt::entity actorEntity = entt::to_entity(event.actor_entity());
	if (event.actor_state() == kActorStateMounted)
	{
		MountSystem::InterruptAndUnmountActor(actorEntity);
	}
///<<< END WRITING YOUR CODE
}
