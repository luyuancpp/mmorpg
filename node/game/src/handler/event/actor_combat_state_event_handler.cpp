#include "actor_combat_state_event_handler.h"
#include "logic/event/actor_combat_state_event.pb.h"
#include "thread_local/storage.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
void ActorCombatStateEventHandler::Register()
{
		tls.dispatcher.sink<SilenceAddedPbEvent>().connect<&ActorCombatStateEventHandler::SilenceAddedPbEventHandler>();
		tls.dispatcher.sink<SilenceRemovedEvent>().connect<&ActorCombatStateEventHandler::SilenceRemovedEventHandler>();
}

void ActorCombatStateEventHandler::UnRegister()
{
		tls.dispatcher.sink<SilenceAddedPbEvent>().disconnect<&ActorCombatStateEventHandler::SilenceAddedPbEventHandler>();
		tls.dispatcher.sink<SilenceRemovedEvent>().disconnect<&ActorCombatStateEventHandler::SilenceRemovedEventHandler>();
}

void ActorCombatStateEventHandler::SilenceAddedPbEventHandler(const SilenceAddedPbEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ActorCombatStateEventHandler::SilenceRemovedEventHandler(const SilenceRemovedEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

