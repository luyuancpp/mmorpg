#include "skill_event_handler.h"
#include "proto/logic/event/skill_event.pb.h"
#include "thread_local/storage.h"

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE


void SkillEventHandler::Register()
{
    tls.dispatcher.sink<SkillExecutedEvent>().connect<&SkillEventHandler::SkillExecutedEventHandler>();
}

void SkillEventHandler::UnRegister()
{
    tls.dispatcher.sink<SkillExecutedEvent>().disconnect<&SkillEventHandler::SkillExecutedEventHandler>();
}
void SkillEventHandler::SkillExecutedEventHandler(const SkillExecutedEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
