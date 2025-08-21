#include "npc_event_handler.h"
#include "proto/logic/event/npc_event.pb.h"
#include "thread_local/storage.h"

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE


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
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
