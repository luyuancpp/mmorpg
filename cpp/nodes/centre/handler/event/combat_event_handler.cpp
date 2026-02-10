#include "combat_event_handler.h"
#include "proto/common/event/combat_event.pb.h"
#include "threading/dispatcher_manager.h"

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE


void CombatEventHandler::Register()
{
    dispatcher.sink<BeKillEvent>().connect<&CombatEventHandler::BeKillEventHandler>();
}

void CombatEventHandler::UnRegister()
{
    dispatcher.sink<BeKillEvent>().disconnect<&CombatEventHandler::BeKillEventHandler>();
}
void CombatEventHandler::BeKillEventHandler(const BeKillEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
