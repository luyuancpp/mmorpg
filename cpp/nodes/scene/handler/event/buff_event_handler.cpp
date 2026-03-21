#include "buff_event_handler.h"
#include "thread_context/dispatcher_manager.h"

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
void BuffEventHandler::Register()
{
    dispatcher.sink<BuffTestEvet>().connect<&BuffEventHandler::BuffTestEvetHandler>();
}

void BuffEventHandler::UnRegister()
{
    dispatcher.sink<BuffTestEvet>().disconnect<&BuffEventHandler::BuffTestEvetHandler>();
}
void BuffEventHandler::BuffTestEvetHandler(const BuffTestEvet& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
