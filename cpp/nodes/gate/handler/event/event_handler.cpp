#include "event_handler.h"

#include "gate_event_handler.h"
#include "player_event_handler.h"

void EventHandler::Register()
{
	GateEventHandler::Register();
	PlayerEventHandler::Register();
}

void EventHandler::UnRegister()
{
	GateEventHandler::UnRegister();
	PlayerEventHandler::UnRegister();
}