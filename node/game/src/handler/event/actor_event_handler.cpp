#include "actor_event_handler.h"
#include "logic/event/actor_event.pb.h"
#include "thread_local/storage.h"
///<<< BEGIN WRITING YOUR CODE
#include "game_logic/actor/actionstate/constants/actor_state_constants.h"
#include "game_logic/actor/actionstate/util/actor_action_state_util.h"
#include "game_logic/actor/attribute/util/actor_attribute_calculator_util.h"
#include "game_logic/actor/attribute/util/actor_state_attribute_sync_util.h"
#include "game_logic/combat/buff/util/buff_util.h"
#include "game_logic/combat/skill/util/skill_util.h"
#include "game_logic/mount/util/mount_util.h"
#include "game_logic/scene/util/interest_util.h"
///<<< END WRITING YOUR CODE
void ActorEventHandler::Register()
{
		tls.dispatcher.sink<InitializeActorComponentsEvent>().connect<&ActorEventHandler::InitializeActorComponentsEventHandler>();
		tls.dispatcher.sink<InterruptCurrentStatePbEvent>().connect<&ActorEventHandler::InterruptCurrentStatePbEventHandler>();
}

void ActorEventHandler::UnRegister()
{
		tls.dispatcher.sink<InitializeActorComponentsEvent>().disconnect<&ActorEventHandler::InitializeActorComponentsEventHandler>();
		tls.dispatcher.sink<InterruptCurrentStatePbEvent>().disconnect<&ActorEventHandler::InterruptCurrentStatePbEventHandler>();
}

void ActorEventHandler::InitializeActorComponentsEventHandler(const InitializeActorComponentsEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
	entt::entity actorEntity = entt::to_entity(event.actor_entity());

	ActorStateAttributeSyncUtil::InitializeActorComponents(actorEntity);
	BuffUtil::InitializeActorComponents(actorEntity);
	InterestUtil::InitializeActorComponents(actorEntity);
	ActorAttributeCalculatorUtil::InitializeActorComponents(actorEntity);
	ActorActionStateUtil::InitializeActorComponents(actorEntity);
///<<< END WRITING YOUR CODE
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

