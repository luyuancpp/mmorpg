#include "buff_event_handler.h"
#include "proto/common/event/buff_event.pb.h"
#include "threading/dispatcher_manager.h"

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
