#include "mission_async_event_receiver.h"
#include "event_proto/mission_event.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
void MissionAsyncEventReceiverReceiver::Register(entt::dispatcher& dispatcher)
{
dispatcher.sink<AcceptMissionEvent>().connect<&MissionAsyncEventReceiverReceiver::MissionAsyncEventReceiverReceiver::Receive0>();
dispatcher.sink<MissionConditionEvent>().connect<&MissionAsyncEventReceiverReceiver::MissionAsyncEventReceiverReceiver::Receive1>();
dispatcher.sink<OnAcceptedMissionEvent>().connect<&MissionAsyncEventReceiverReceiver::MissionAsyncEventReceiverReceiver::Receive2>();
dispatcher.sink<OnMissionAwardEvent>().connect<&MissionAsyncEventReceiverReceiver::MissionAsyncEventReceiverReceiver::Receive3>();
}

void MissionAsyncEventReceiverReceiver::UnRegister(entt::dispatcher& dispatcher)
{
dispatcher.sink<AcceptMissionEvent>().disconnect<&MissionAsyncEventReceiverReceiver::MissionAsyncEventReceiverReceiver::Receive0>();
dispatcher.sink<MissionConditionEvent>().disconnect<&MissionAsyncEventReceiverReceiver::MissionAsyncEventReceiverReceiver::Receive1>();
dispatcher.sink<OnAcceptedMissionEvent>().disconnect<&MissionAsyncEventReceiverReceiver::MissionAsyncEventReceiverReceiver::Receive2>();
dispatcher.sink<OnMissionAwardEvent>().disconnect<&MissionAsyncEventReceiverReceiver::MissionAsyncEventReceiverReceiver::Receive3>();
}

void MissionAsyncEventReceiverReceiver::Receive0(const AcceptMissionEvent& event_obj)
{
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE 
}

void MissionAsyncEventReceiverReceiver::Receive1(const MissionConditionEvent& event_obj)
{
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE 
}

void MissionAsyncEventReceiverReceiver::Receive2(const OnAcceptedMissionEvent& event_obj)
{
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE 
}

void MissionAsyncEventReceiverReceiver::Receive3(const OnMissionAwardEvent& event_obj)
{
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE 
}

