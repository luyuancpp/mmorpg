#include "mission_event_handler.h"
#include "thread_context/dispatcher_manager.h"

void MissionEventHandler::Register()
{
    dispatcher.sink<AcceptMissionEvent>().connect<&MissionEventHandler::AcceptMissionEventHandler>();
    dispatcher.sink<MissionConditionEvent>().connect<&MissionEventHandler::MissionConditionEventHandler>();
    dispatcher.sink<OnAcceptedMissionEvent>().connect<&MissionEventHandler::OnAcceptedMissionEventHandler>();
    dispatcher.sink<OnMissionAwardEvent>().connect<&MissionEventHandler::OnMissionAwardEventHandler>();
}

void MissionEventHandler::UnRegister()
{
    dispatcher.sink<AcceptMissionEvent>().disconnect<&MissionEventHandler::AcceptMissionEventHandler>();
    dispatcher.sink<MissionConditionEvent>().disconnect<&MissionEventHandler::MissionConditionEventHandler>();
    dispatcher.sink<OnAcceptedMissionEvent>().disconnect<&MissionEventHandler::OnAcceptedMissionEventHandler>();
    dispatcher.sink<OnMissionAwardEvent>().disconnect<&MissionEventHandler::OnMissionAwardEventHandler>();
}
void MissionEventHandler::AcceptMissionEventHandler(const AcceptMissionEvent& event)
{

}
void MissionEventHandler::MissionConditionEventHandler(const MissionConditionEvent& event)
{

}
void MissionEventHandler::OnAcceptedMissionEventHandler(const OnAcceptedMissionEvent& event)
{

}
void MissionEventHandler::OnMissionAwardEventHandler(const OnMissionAwardEvent& event)
{

}

