#include "scene_event_receiver.h"
#include "event_proto/scene_event.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

void SceneEventReceiverReceiver::Register(entt::dispatcher& dispatcher)
{
dispatcher.sink<BeforeEnterScene>().connect<&SceneEventReceiverReceiver::SceneEventReceiverReceiver::Receive0>();
dispatcher.sink<OnEnterScene>().connect<&SceneEventReceiverReceiver::SceneEventReceiverReceiver::Receive1>();
dispatcher.sink<BeforeLeaveScene>().connect<&SceneEventReceiverReceiver::SceneEventReceiverReceiver::Receive2>();
dispatcher.sink<OnLeaveScene>().connect<&SceneEventReceiverReceiver::SceneEventReceiverReceiver::Receive3>();
}

void SceneEventReceiverReceiver::UnRegister(entt::dispatcher& dispatcher)
{
dispatcher.sink<BeforeEnterScene>().disconnect<&SceneEventReceiverReceiver::SceneEventReceiverReceiver::Receive0>();
dispatcher.sink<OnEnterScene>().disconnect<&SceneEventReceiverReceiver::SceneEventReceiverReceiver::Receive1>();
dispatcher.sink<BeforeLeaveScene>().disconnect<&SceneEventReceiverReceiver::SceneEventReceiverReceiver::Receive2>();
dispatcher.sink<OnLeaveScene>().disconnect<&SceneEventReceiverReceiver::SceneEventReceiverReceiver::Receive3>();
}

void SceneEventReceiverReceiver::Receive0(const BeforeEnterScene& event_obj)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
void SceneEventReceiverReceiver::Receive1(const OnEnterScene& event_obj)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
void SceneEventReceiverReceiver::Receive2(const BeforeLeaveScene& event_obj)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
void SceneEventReceiverReceiver::Receive3(const OnLeaveScene& event_obj)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
