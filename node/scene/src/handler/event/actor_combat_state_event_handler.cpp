#include "actor_combat_state_event_handler.h"
#include "proto/logic/event/actor_combat_state_event.pb.h"
#include "thread_local/storage.h"

///<<< BEGIN WRITING YOUR CODE
#include "actor/combat_state/system/combat_state_system.h"
#include <thread_local/dispatcher_manager.h>
///<<< END WRITING YOUR CODE


void ActorCombatStateEventHandler::Register()
{
    dispatcher.sink<CombatStateAddedPbEvent>().connect<&ActorCombatStateEventHandler::CombatStateAddedPbEventHandler>();
    dispatcher.sink<CombatStateRemovedPbEvent>().connect<&ActorCombatStateEventHandler::CombatStateRemovedPbEventHandler>();
}

void ActorCombatStateEventHandler::UnRegister()
{
    dispatcher.sink<CombatStateAddedPbEvent>().disconnect<&ActorCombatStateEventHandler::CombatStateAddedPbEventHandler>();
    dispatcher.sink<CombatStateRemovedPbEvent>().disconnect<&ActorCombatStateEventHandler::CombatStateRemovedPbEventHandler>();
}
void ActorCombatStateEventHandler::CombatStateAddedPbEventHandler(const CombatStateAddedPbEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
	CombatStateSystem::AddCombatState(event);
///<<< END WRITING YOUR CODE
}
void ActorCombatStateEventHandler::CombatStateRemovedPbEventHandler(const CombatStateRemovedPbEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
	CombatStateSystem::RemoveCombatState(event);
///<<< END WRITING YOUR CODE
}
