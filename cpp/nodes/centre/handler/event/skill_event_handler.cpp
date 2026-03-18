#include "skill_event_handler.h"
#include "thread_context/dispatcher_manager.h"

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

}

