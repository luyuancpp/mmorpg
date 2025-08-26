#include "skill_event_handler.h"
#include "proto/logic/event/skill_event.pb.h"
#include "threading/dispatcher_manager.h"

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE


void SkillEventHandler::Register()
{
    dispatcher.sink<SkillExecutedEvent>().connect<&SkillEventHandler::SkillExecutedEventHandler>();
}

void SkillEventHandler::UnRegister()
{
    dispatcher.sink<SkillExecutedEvent>().disconnect<&SkillEventHandler::SkillExecutedEventHandler>();
}
void SkillEventHandler::SkillExecutedEventHandler(const SkillExecutedEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
