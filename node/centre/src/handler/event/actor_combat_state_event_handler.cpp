#include "actor_combat_state_event_handler.h"
#include "proto/logic/event/actor_combat_state_event.pb.h"
#include "thread_local/storage.h"

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE


void ActorCombatStateEventHandler::Register()
{
    tls.dispatcher.sink<CombatStateAddedPbEvent>().connect<&ActorCombatStateEventHandler::CombatStateAddedPbEventHandler>();
    tls.dispatcher.sink<CombatStateRemovedPbEvent>().connect<&ActorCombatStateEventHandler::CombatStateRemovedPbEventHandler>();
}

void ActorCombatStateEventHandler::UnRegister()
{
    tls.dispatcher.sink<CombatStateAddedPbEvent>().disconnect<&ActorCombatStateEventHandler::CombatStateAddedPbEventHandler>();
    tls.dispatcher.sink<CombatStateRemovedPbEvent>().disconnect<&ActorCombatStateEventHandler::CombatStateRemovedPbEventHandler>();
}
void ActorCombatStateEventHandler::CombatStateAddedPbEventHandler(const CombatStateAddedPbEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
void ActorCombatStateEventHandler::CombatStateRemovedPbEventHandler(const CombatStateRemovedPbEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
