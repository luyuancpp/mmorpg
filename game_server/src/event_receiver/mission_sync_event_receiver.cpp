#include "mission_sync_event_receiver.h"
#include "event_proto/mission_event.pb.h"
///<<< BEGIN WRITING YOUR CODE
#include "src/game_logic/game_registry.h"
#include "src/game_logic/missions/missions_base.h"
///<<< END WRITING YOUR CODE
void MissionSyncEventReceiverReceiver::Register(entt::dispatcher& dispatcher)
{
dispatcher.sink<AcceptMissionEvent>().connect<&MissionSyncEventReceiverReceiver::MissionSyncEventReceiverReceiver::Receive0>();
dispatcher.sink<MissionConditionEvent>().connect<&MissionSyncEventReceiverReceiver::MissionSyncEventReceiverReceiver::Receive1>();
dispatcher.sink<OnAcceptedMissionEvent>().connect<&MissionSyncEventReceiverReceiver::MissionSyncEventReceiverReceiver::Receive2>();
dispatcher.sink<OnMissionAwardEvent>().connect<&MissionSyncEventReceiverReceiver::MissionSyncEventReceiverReceiver::Receive3>();
}

void MissionSyncEventReceiverReceiver::UnRegister(entt::dispatcher& dispatcher)
{
dispatcher.sink<AcceptMissionEvent>().disconnect<&MissionSyncEventReceiverReceiver::MissionSyncEventReceiverReceiver::Receive0>();
dispatcher.sink<MissionConditionEvent>().disconnect<&MissionSyncEventReceiverReceiver::MissionSyncEventReceiverReceiver::Receive1>();
dispatcher.sink<OnAcceptedMissionEvent>().disconnect<&MissionSyncEventReceiverReceiver::MissionSyncEventReceiverReceiver::Receive2>();
dispatcher.sink<OnMissionAwardEvent>().disconnect<&MissionSyncEventReceiverReceiver::MissionSyncEventReceiverReceiver::Receive3>();
}

void MissionSyncEventReceiverReceiver::Receive0(const AcceptMissionEvent& event_obj)
{
///<<< BEGIN WRITING YOUR CODE 
    auto entity = entt::to_entity(event_obj.entity());
    registry.get<MissionsComp>(entity).Accept(event_obj);
///<<< END WRITING YOUR CODE 
}

void MissionSyncEventReceiverReceiver::Receive1(const MissionConditionEvent& event_obj)
{
///<<< BEGIN WRITING YOUR CODE 
    auto entity = entt::to_entity(event_obj.entity());
    registry.get<MissionsComp>(entity).Receive(event_obj);
///<<< END WRITING YOUR CODE 
}

void MissionSyncEventReceiverReceiver::Receive2(const OnAcceptedMissionEvent& event_obj)
{
///<<< BEGIN WRITING YOUR CODE 
 //触发接任务自动匹配当前的任务进度,接受的时候已经拥有金币
///<<< END WRITING YOUR CODE 
}

void MissionSyncEventReceiverReceiver::Receive3(const OnMissionAwardEvent& event_obj)
{
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE 
}

