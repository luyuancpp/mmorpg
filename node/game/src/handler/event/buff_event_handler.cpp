#include "buff_event_handler.h"
#include "logic/event/buff_event.pb.h"
#include "thread_local/storage.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
void BuffEventHandler::Register()
{
		tls.dispatcher.sink<AbilityExecutedEvent>().connect<&BuffEventHandler::AbilityExecutedEventHandler>();
}

void BuffEventHandler::UnRegister()
{
		tls.dispatcher.sink<AbilityExecutedEvent>().disconnect<&BuffEventHandler::AbilityExecutedEventHandler>();
}

void BuffEventHandler::AbilityExecutedEventHandler(const AbilityExecutedEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

