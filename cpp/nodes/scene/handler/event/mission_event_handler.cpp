#include "mission_event_handler.h"
#include "proto/logic/event/mission_event.pb.h"
#include "threading/dispatcher_manager.h"

///<<< BEGIN WRITING YOUR CODE
#include "modules/mission/comp/mission_comp.h"
#include "modules/mission/system/mission.h"
///<<< END WRITING YOUR CODE


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
///<<< BEGIN WRITING YOUR CODE
	MissionSystem::AcceptMission(event);
///<<< END WRITING YOUR CODE
}
void MissionEventHandler::MissionConditionEventHandler(const MissionConditionEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
    MissionSystem::HandleMissionConditionEvent(event);
///<<< END WRITING YOUR CODE
}
void MissionEventHandler::OnAcceptedMissionEventHandler(const OnAcceptedMissionEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
     //触发接任务自动匹配当前的任务进度,接受的时候已经拥有金币刷新任务的进度
///<<< END WRITING YOUR CODE
}
void MissionEventHandler::OnMissionAwardEventHandler(const OnMissionAwardEvent& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
