#include "mission_event_receiver.h"
#include "event_proto/mission_event.pb.h"
///<<< BEGIN WRITING YOUR CODE
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/game_logic/missions/missions_base.h"
///<<< END WRITING YOUR CODE
void MissionEventReceiverReceiver::Register(entt::dispatcher& dispatcher)
{
dispatcher.sink<AcceptMissionEvent>().connect<&MissionEventReceiverReceiver::MissionEventReceiverReceiver::Receive0>();
dispatcher.sink<MissionConditionEvent>().connect<&MissionEventReceiverReceiver::MissionEventReceiverReceiver::Receive1>();
dispatcher.sink<OnAcceptedMissionEvent>().connect<&MissionEventReceiverReceiver::MissionEventReceiverReceiver::Receive2>();
dispatcher.sink<OnMissionAwardEvent>().connect<&MissionEventReceiverReceiver::MissionEventReceiverReceiver::Receive3>();
}

void MissionEventReceiverReceiver::UnRegister(entt::dispatcher& dispatcher)
{
dispatcher.sink<AcceptMissionEvent>().disconnect<&MissionEventReceiverReceiver::MissionEventReceiverReceiver::Receive0>();
dispatcher.sink<MissionConditionEvent>().disconnect<&MissionEventReceiverReceiver::MissionEventReceiverReceiver::Receive1>();
dispatcher.sink<OnAcceptedMissionEvent>().disconnect<&MissionEventReceiverReceiver::MissionEventReceiverReceiver::Receive2>();
dispatcher.sink<OnMissionAwardEvent>().disconnect<&MissionEventReceiverReceiver::MissionEventReceiverReceiver::Receive3>();
}

void MissionEventReceiverReceiver::Receive0(const AcceptMissionEvent& event_obj)
{
///<<< BEGIN WRITING YOUR CODE 
	auto entity = entt::to_entity(event_obj.entity());
	tls.registry.get<MissionsComp>(entity).Accept(event_obj);
///<<< END WRITING YOUR CODE 
}

void MissionEventReceiverReceiver::Receive1(const MissionConditionEvent& event_obj)
{
///<<< BEGIN WRITING YOUR CODE 
	auto entity = entt::to_entity(event_obj.entity());
	tls.registry.get<MissionsComp>(entity).Receive(event_obj);
///<<< END WRITING YOUR CODE 
}

void MissionEventReceiverReceiver::Receive2(const OnAcceptedMissionEvent& event_obj)
{
///<<< BEGIN WRITING YOUR CODE 
	//触发接任务自动匹配当前的任务进度,接受的时候已经拥有金币
///<<< END WRITING YOUR CODE 
}

void MissionEventReceiverReceiver::Receive3(const OnMissionAwardEvent& event_obj)
{
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE 
}

