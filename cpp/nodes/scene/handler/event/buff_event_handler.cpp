#include "buff_event_handler.h"
#include "thread_context/dispatcher_manager.h"

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
void BuffEventHandler::Register()
{
    tlsEcs.dispatcher.sink<BuffTestEvent>().connect<&BuffEventHandler::BuffTestEventHandler>();
}

void BuffEventHandler::UnRegister()
{
    tlsEcs.dispatcher.sink<BuffTestEvent>().disconnect<&BuffEventHandler::BuffTestEventHandler>();
}
void BuffEventHandler::BuffTestEventHandler(const BuffTestEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
