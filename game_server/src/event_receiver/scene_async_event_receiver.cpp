#include "scene_async_event_receiver.h"
#include "event_proto/scene_event.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
void SceneAsyncEventReceiverReceiver::Register(entt::dispatcher& dispatcher)
{
dispatcher.sink<BeforeEnterScene>().connect<&SceneAsyncEventReceiverReceiver::SceneAsyncEventReceiverReceiver::Receive0>();
dispatcher.sink<OnEnterScene>().connect<&SceneAsyncEventReceiverReceiver::SceneAsyncEventReceiverReceiver::Receive1>();
dispatcher.sink<BeforeLeaveScene>().connect<&SceneAsyncEventReceiverReceiver::SceneAsyncEventReceiverReceiver::Receive2>();
dispatcher.sink<OnLeaveScene>().connect<&SceneAsyncEventReceiverReceiver::SceneAsyncEventReceiverReceiver::Receive3>();
}

void SceneAsyncEventReceiverReceiver::UnRegister(entt::dispatcher& dispatcher)
{
dispatcher.sink<BeforeEnterScene>().disconnect<&SceneAsyncEventReceiverReceiver::SceneAsyncEventReceiverReceiver::Receive0>();
dispatcher.sink<OnEnterScene>().disconnect<&SceneAsyncEventReceiverReceiver::SceneAsyncEventReceiverReceiver::Receive1>();
dispatcher.sink<BeforeLeaveScene>().disconnect<&SceneAsyncEventReceiverReceiver::SceneAsyncEventReceiverReceiver::Receive2>();
dispatcher.sink<OnLeaveScene>().disconnect<&SceneAsyncEventReceiverReceiver::SceneAsyncEventReceiverReceiver::Receive3>();
}

void SceneAsyncEventReceiverReceiver::Receive0(const BeforeEnterScene& event_obj)
{
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE 
}

void SceneAsyncEventReceiverReceiver::Receive1(const OnEnterScene& event_obj)
{
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE 
}

void SceneAsyncEventReceiverReceiver::Receive2(const BeforeLeaveScene& event_obj)
{
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE 
}

void SceneAsyncEventReceiverReceiver::Receive3(const OnLeaveScene& event_obj)
{
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE 
}

