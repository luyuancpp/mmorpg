#include "actor_combat_state_event_handler.h"
#include "thread_context/ecs_context.h"

///<<< BEGIN WRITING YOUR CODE
#include "combat_state/system/combat_state.h"
///<<< END WRITING YOUR CODE
void ActorCombatStateEventHandler::Register()
{
    tlsEcs.dispatcher.sink<CombatStateAddedEvent>().connect<&ActorCombatStateEventHandler::CombatStateAddedEventHandler>();
    tlsEcs.dispatcher.sink<CombatStateRemovedEvent>().connect<&ActorCombatStateEventHandler::CombatStateRemovedEventHandler>();
}

void ActorCombatStateEventHandler::UnRegister()
{
    tlsEcs.dispatcher.sink<CombatStateAddedEvent>().disconnect<&ActorCombatStateEventHandler::CombatStateAddedEventHandler>();
    tlsEcs.dispatcher.sink<CombatStateRemovedEvent>().disconnect<&ActorCombatStateEventHandler::CombatStateRemovedEventHandler>();
}
void ActorCombatStateEventHandler::CombatStateAddedEventHandler(const CombatStateAddedEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
	CombatStateSystem::AddCombatState(event);
///<<< END WRITING YOUR CODE
}
void ActorCombatStateEventHandler::CombatStateRemovedEventHandler(const CombatStateRemovedEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
	CombatStateSystem::RemoveCombatState(event);
///<<< END WRITING YOUR CODE
}
