#include "buff_event_handler.h"
#include "logic/event/buff_event.pb.h"
#include "thread_local/storage.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
void BuffEventHandler::Register()
{
		tls.dispatcher.sink<SkillExecutedEvent>().connect<&BuffEventHandler::SkillExecutedEventHandler>();
		tls.dispatcher.sink<BeforeGiveDamageEvent>().connect<&BuffEventHandler::BeforeGiveDamageEventHandler>();
}

void BuffEventHandler::UnRegister()
{
		tls.dispatcher.sink<SkillExecutedEvent>().disconnect<&BuffEventHandler::SkillExecutedEventHandler>();
		tls.dispatcher.sink<BeforeGiveDamageEvent>().disconnect<&BuffEventHandler::BeforeGiveDamageEventHandler>();
}

void BuffEventHandler::SkillExecutedEventHandler(const SkillExecutedEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void BuffEventHandler::BeforeGiveDamageEventHandler(const BeforeGiveDamageEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

