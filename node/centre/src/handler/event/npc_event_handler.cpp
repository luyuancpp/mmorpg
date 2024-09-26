#include "npc_event_handler.h"
#include "logic/event/npc_event.pb.h"
#include "thread_local/storage.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
void NpcEventHandler::Register()
{
		tls.dispatcher.sink<InitializeNpcComponents>().connect<&NpcEventHandler::InitializeNpcComponentsHandler>();
}

void NpcEventHandler::UnRegister()
{
		tls.dispatcher.sink<InitializeNpcComponents>().disconnect<&NpcEventHandler::InitializeNpcComponentsHandler>();
}

void NpcEventHandler::InitializeNpcComponentsHandler(const InitializeNpcComponents& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

