#include "actor_event_handler.h"
#include "thread_context/dispatcher_manager.h"

///<<< BEGIN WRITING YOUR CODE
#include "actor/action_state/constants/actor_state.h"
#include "actor/action_state/system/actor_action_state.h"
#include "actor/attribute/system/actor_attribute_calculator.h"
#include "actor/attribute/system/actor_state_attribute_sync.h"
#include "combat_state/system/combat_state.h"
#include "combat/buff/system/buff.h"
#include "combat/skill/system/skill.h"
#include "core/utils/registry/game_registry.h"
#include "spatial/system/interest.h"
///<<< END WRITING YOUR CODE
void ActorEventHandler::Register()
{
    tlsEcs.dispatcher.sink<InitializeActorCompsEvent>().connect<&ActorEventHandler::InitializeActorCompsEventHandler>();
    tlsEcs.dispatcher.sink<InterruptCurrentStateEvent>().connect<&ActorEventHandler::InterruptCurrentStateEventHandler>();
}

void ActorEventHandler::UnRegister()
{
    tlsEcs.dispatcher.sink<InitializeActorCompsEvent>().disconnect<&ActorEventHandler::InitializeActorCompsEventHandler>();
    tlsEcs.dispatcher.sink<InterruptCurrentStateEvent>().disconnect<&ActorEventHandler::InterruptCurrentStateEventHandler>();
}
void ActorEventHandler::InitializeActorCompsEventHandler(const InitializeActorCompsEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
void ActorEventHandler::InterruptCurrentStateEventHandler(const InterruptCurrentStateEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
