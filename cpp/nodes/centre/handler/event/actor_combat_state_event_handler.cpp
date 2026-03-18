#include "actor_combat_state_event_handler.h"
#include "thread_context/dispatcher_manager.h"

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

}
void ActorCombatStateEventHandler::CombatStateRemovedPbEventHandler(const CombatStateRemovedPbEvent& event)
{

}

