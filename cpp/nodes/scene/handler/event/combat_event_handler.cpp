#include "combat_event_handler.h"
#include "thread_context/ecs_context.h"

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
void CombatEventHandler::Register()
{
    tlsEcs.dispatcher.sink<BeKillEvent>().connect<&CombatEventHandler::BeKillEventHandler>();
}

void CombatEventHandler::UnRegister()
{
    tlsEcs.dispatcher.sink<BeKillEvent>().disconnect<&CombatEventHandler::BeKillEventHandler>();
}
void CombatEventHandler::BeKillEventHandler(const BeKillEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
