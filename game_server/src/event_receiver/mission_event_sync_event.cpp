#include "mission_event_sync_event.h"
#include "event_proto/mission_event.pb.h"
///<<< BEGIN WRITING YOUR CODE
#include "src/game_logic/game_registry.h"
#include "src/game_logic/missions/missions_base.h"
///<<< END WRITING YOUR CODE
void MissionEventReceiver::Register(entt::dispatcher& dispatcher)
{
dispatcher.sink<AcceptMissionEvent>().connect<&MissionEventReceiver::MissionEventReceiver::Receive0>();
dispatcher.sink<MissionConditionEvent>().connect<&MissionEventReceiver::MissionEventReceiver::Receive1>();
dispatcher.sink<OnAcceptedMissionEvent>().connect<&MissionEventReceiver::MissionEventReceiver::Receive2>();
dispatcher.sink<OnMissionAwardEvent>().connect<&MissionEventReceiver::MissionEventReceiver::Receive3>();
}

void MissionEventReceiver::UnRegister(entt::dispatcher& dispatcher)
{
dispatcher.sink<AcceptMissionEvent>().disconnect<&MissionEventReceiver::MissionEventReceiver::Receive0>();
dispatcher.sink<MissionConditionEvent>().disconnect<&MissionEventReceiver::MissionEventReceiver::Receive1>();
dispatcher.sink<OnAcceptedMissionEvent>().disconnect<&MissionEventReceiver::MissionEventReceiver::Receive2>();
dispatcher.sink<OnMissionAwardEvent>().disconnect<&MissionEventReceiver::MissionEventReceiver::Receive3>();
}

void MissionEventReceiver::Receive0(const AcceptMissionEvent& event_obj)
{
///<<< BEGIN WRITING YOUR CODE 
	auto entity = entt::to_entity(event_obj.entity());
	registry.get<MissionsComp>(entity).Accept(event_obj);
///<<< END WRITING YOUR CODE 
}

void MissionEventReceiver::Receive1(const MissionConditionEvent& event_obj)
{
///<<< BEGIN WRITING YOUR CODE 
	auto entity = entt::to_entity(event_obj.entity());
	registry.get<MissionsComp>(entity).Receive(event_obj);
///<<< END WRITING YOUR CODE 
}

void MissionEventReceiver::Receive2(const OnAcceptedMissionEvent& event_obj)
{
///<<< BEGIN WRITING YOUR CODE 
	//触发接任务自动匹配当前的任务进度,接受的时候已经拥有金币
///<<< END WRITING YOUR CODE 
}

void MissionEventReceiver::Receive3(const OnMissionAwardEvent& event_obj)
{
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE 
}

