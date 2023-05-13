#include "mission_event_handler.h"
#include "event_proto/mission_event.pb.h"
///<<< BEGIN WRITING YOUR CODE
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/game_logic/missions/missions_base.h"
///<<< END WRITING YOUR CODE
void MissionEventHandler::Register()
{
		tls.dispatcher.sink<AcceptMissionEvent>().connect<&MissionEventHandler::AcceptMissionEventHandler>();
		tls.dispatcher.sink<MissionConditionEvent>().connect<&MissionEventHandler::MissionConditionEventHandler>();
		tls.dispatcher.sink<OnAcceptedMissionEvent>().connect<&MissionEventHandler::OnAcceptedMissionEventHandler>();
		tls.dispatcher.sink<OnMissionAwardEvent>().connect<&MissionEventHandler::OnMissionAwardEventHandler>();
}

void MissionEventHandler::UnRegister()
{
		tls.dispatcher.sink<AcceptMissionEvent>().disconnect<&MissionEventHandler::AcceptMissionEventHandler>();
		tls.dispatcher.sink<MissionConditionEvent>().disconnect<&MissionEventHandler::MissionConditionEventHandler>();
		tls.dispatcher.sink<OnAcceptedMissionEvent>().disconnect<&MissionEventHandler::OnAcceptedMissionEventHandler>();
		tls.dispatcher.sink<OnMissionAwardEvent>().disconnect<&MissionEventHandler::OnMissionAwardEventHandler>();
}

void MissionEventHandler::AcceptMissionEventHandler(const AcceptMissionEvent& message)
{
///<<< BEGIN WRITING YOUR CODE
    auto entity = entt::to_entity(message.entity());
    tls.registry.get<MissionsComp>(entity).Accept(message);
///<<< END WRITING YOUR CODE
}

void MissionEventHandler::MissionConditionEventHandler(const MissionConditionEvent& message)
{
///<<< BEGIN WRITING YOUR CODE
    auto entity = entt::to_entity(message.entity());
    tls.registry.get<MissionsComp>(entity).Receive(message);
///<<< END WRITING YOUR CODE
}

void MissionEventHandler::OnAcceptedMissionEventHandler(const OnAcceptedMissionEvent& message)
{
///<<< BEGIN WRITING YOUR CODE
        //触发接任务自动匹配当前的任务进度,接受的时候已经拥有金币
///<<< END WRITING YOUR CODE
}

void MissionEventHandler::OnMissionAwardEventHandler(const OnMissionAwardEvent& message)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

