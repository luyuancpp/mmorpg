#include "buff_event_handler.h"
#include "proto/logic/event/buff_event.pb.h"
#include "thread_local/storage.h"

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE


void BuffEventHandler::Register()
{
    tls.dispatcher.sink<BuffTestEvet>().connect<&BuffEventHandler::BuffTestEvetHandler>();
}

void BuffEventHandler::UnRegister()
{
    tls.dispatcher.sink<BuffTestEvet>().disconnect<&BuffEventHandler::BuffTestEvetHandler>();
}
void BuffEventHandler::BuffTestEvetHandler(const BuffTestEvet& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
