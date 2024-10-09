#include "buff_event_handler.h"
#include "logic/event/buff_event.pb.h"
#include "thread_local/storage.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
void BuffEventHandler::Register()
{
		tls.dispatcher.sink<SkillExecutedEvent>().connect<&BuffEventHandler::SkillExecutedEventHandler>();
		tls.dispatcher.sink<BeforeGiveDamageEvent>().connect<&BuffEventHandler::BeforeGiveDamageEventHandler>();
		tls.dispatcher.sink<AfterGiveDamageEvent>().connect<&BuffEventHandler::AfterGiveDamageEventHandler>();
		tls.dispatcher.sink<BeforeTakeDamageEvent>().connect<&BuffEventHandler::BeforeTakeDamageEventHandler>();
		tls.dispatcher.sink<AfterTakeDamageEvent>().connect<&BuffEventHandler::AfterTakeDamageEventHandler>();
		tls.dispatcher.sink<BeforeDeadEvent>().connect<&BuffEventHandler::BeforeDeadEventHandler>();
		tls.dispatcher.sink<AfterDeadEvent>().connect<&BuffEventHandler::AfterDeadEventHandler>();
		tls.dispatcher.sink<KillEvent>().connect<&BuffEventHandler::KillEventHandler>();
}

void BuffEventHandler::UnRegister()
{
		tls.dispatcher.sink<SkillExecutedEvent>().disconnect<&BuffEventHandler::SkillExecutedEventHandler>();
		tls.dispatcher.sink<BeforeGiveDamageEvent>().disconnect<&BuffEventHandler::BeforeGiveDamageEventHandler>();
		tls.dispatcher.sink<AfterGiveDamageEvent>().disconnect<&BuffEventHandler::AfterGiveDamageEventHandler>();
		tls.dispatcher.sink<BeforeTakeDamageEvent>().disconnect<&BuffEventHandler::BeforeTakeDamageEventHandler>();
		tls.dispatcher.sink<AfterTakeDamageEvent>().disconnect<&BuffEventHandler::AfterTakeDamageEventHandler>();
		tls.dispatcher.sink<BeforeDeadEvent>().disconnect<&BuffEventHandler::BeforeDeadEventHandler>();
		tls.dispatcher.sink<AfterDeadEvent>().disconnect<&BuffEventHandler::AfterDeadEventHandler>();
		tls.dispatcher.sink<KillEvent>().disconnect<&BuffEventHandler::KillEventHandler>();
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

void BuffEventHandler::AfterGiveDamageEventHandler(const AfterGiveDamageEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void BuffEventHandler::BeforeTakeDamageEventHandler(const BeforeTakeDamageEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void BuffEventHandler::AfterTakeDamageEventHandler(const AfterTakeDamageEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void BuffEventHandler::BeforeDeadEventHandler(const BeforeDeadEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void BuffEventHandler::AfterDeadEventHandler(const AfterDeadEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void BuffEventHandler::KillEventHandler(const KillEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
