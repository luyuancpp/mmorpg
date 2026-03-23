#include "buff_event_handler.h"
#include "thread_context/dispatcher_manager.h"

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
void BuffEventHandler::Register()
{
    dispatcher.sink<BuffTestEvent>().connect<&BuffEventHandler::BuffTestEventHandler>();
}

void BuffEventHandler::UnRegister()
{
    dispatcher.sink<BuffTestEvent>().disconnect<&BuffEventHandler::BuffTestEventHandler>();
}
void BuffEventHandler::BuffTestEventHandler(const BuffTestEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
