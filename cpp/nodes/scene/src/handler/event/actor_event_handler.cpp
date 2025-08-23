#include "actor_event_handler.h"
#include "proto/logic/event/actor_event.pb.h"
#include "thread_local/dispatcher_manager.h"

///<<< BEGIN WRITING YOUR CODE
#include "actor/action_state/constants/actor_state_constants.h"
#include "actor/action_state/system/actor_action_state_system.h"
#include "actor/attribute/system/actor_attribute_calculator_system.h"
#include "actor/attribute/system/actor_state_attribute_sync_system.h"
#include "actor/combat_state/system/combat_state_system.h"
#include "combat/buff/system/buff_system.h"
#include "combat/skill/system/skill_system.h"
#include "mount/system/mount_system.h"
#include "scene/system/interest_system.h"
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

	ActorStateAttributeSyncSystem::InitializeActorComponents(actorEntity);
	BuffSystem::InitializeActorComponents(actorEntity);
	InterestSystem::InitializeActorComponents(actorEntity);
	ActorAttributeCalculatorSystem::InitializeActorComponents(actorEntity);
	ActorActionStateSystem::InitializeActorComponents(actorEntity);
	CombatStateSystem::InitializeCombatStateComponent(actorEntity);
	SkillSystem::InitializeActorComponents(actorEntity);
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
