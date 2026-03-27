#include "combat_event_handler.h"
#include "thread_context/dispatcher_manager.h"

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

}

