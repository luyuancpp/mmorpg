#include "scene_sync_event_receiver.h"
#include "event_proto/scene_event.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
void SceneSyncEventReceiverReceiver::Register(entt::dispatcher& dispatcher)
{
dispatcher.sink<BeforeEnterScene>().connect<&SceneSyncEventReceiverReceiver::SceneSyncEventReceiverReceiver::Receive0>();
dispatcher.sink<OnEnterScene>().connect<&SceneSyncEventReceiverReceiver::SceneSyncEventReceiverReceiver::Receive1>();
dispatcher.sink<BeforeLeaveScene>().connect<&SceneSyncEventReceiverReceiver::SceneSyncEventReceiverReceiver::Receive2>();
dispatcher.sink<OnLeaveScene>().connect<&SceneSyncEventReceiverReceiver::SceneSyncEventReceiverReceiver::Receive3>();
}

void SceneSyncEventReceiverReceiver::UnRegister(entt::dispatcher& dispatcher)
{
dispatcher.sink<BeforeEnterScene>().disconnect<&SceneSyncEventReceiverReceiver::SceneSyncEventReceiverReceiver::Receive0>();
dispatcher.sink<OnEnterScene>().disconnect<&SceneSyncEventReceiverReceiver::SceneSyncEventReceiverReceiver::Receive1>();
dispatcher.sink<BeforeLeaveScene>().disconnect<&SceneSyncEventReceiverReceiver::SceneSyncEventReceiverReceiver::Receive2>();
dispatcher.sink<OnLeaveScene>().disconnect<&SceneSyncEventReceiverReceiver::SceneSyncEventReceiverReceiver::Receive3>();
}

void SceneSyncEventReceiverReceiver::Receive0(const BeforeEnterScene& event_obj)
{
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE 
}

void SceneSyncEventReceiverReceiver::Receive1(const OnEnterScene& event_obj)
{
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE 
}

void SceneSyncEventReceiverReceiver::Receive2(const BeforeLeaveScene& event_obj)
{
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE 
}

void SceneSyncEventReceiverReceiver::Receive3(const OnLeaveScene& event_obj)
{
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE 
}

