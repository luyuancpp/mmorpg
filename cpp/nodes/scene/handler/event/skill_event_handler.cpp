#include "skill_event_handler.h"
#include "thread_context/ecs_context.h"

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
void SkillEventHandler::Register()
{
    tlsEcs.dispatcher.sink<SkillExecutedEvent>().connect<&SkillEventHandler::SkillExecutedEventHandler>();
}

void SkillEventHandler::UnRegister()
{
    tlsEcs.dispatcher.sink<SkillExecutedEvent>().disconnect<&SkillEventHandler::SkillExecutedEventHandler>();
}
void SkillEventHandler::SkillExecutedEventHandler(const SkillExecutedEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
