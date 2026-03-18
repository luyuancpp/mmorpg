#include "npc_event_handler.h"
#include "thread_context/dispatcher_manager.h"

void NpcEventHandler::Register()
{
    dispatcher.sink<InitializeNpcComponentsEvent>().connect<&NpcEventHandler::InitializeNpcComponentsEventHandler>();
}

void NpcEventHandler::UnRegister()
{
    dispatcher.sink<InitializeNpcComponentsEvent>().disconnect<&NpcEventHandler::InitializeNpcComponentsEventHandler>();
}
void NpcEventHandler::InitializeNpcComponentsEventHandler(const InitializeNpcComponentsEvent& event)
{

}

