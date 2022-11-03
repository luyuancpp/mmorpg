#include "scene_event_sync_event.h"
#include "event_proto/scene_event.pb.h"
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE
void SceneEventReceiver::Register(entt::dispatcher& dispatcher)
{
dispatcher.sink<BeforeEnterScene>().connect<&SceneEventReceiver::SceneEventReceiver::Receive0>();
dispatcher.sink<OnEnterScene>().connect<&SceneEventReceiver::SceneEventReceiver::Receive1>();
dispatcher.sink<BeforeLeaveScene>().connect<&SceneEventReceiver::SceneEventReceiver::Receive2>();
dispatcher.sink<OnLeaveScene>().connect<&SceneEventReceiver::SceneEventReceiver::Receive3>();
}

void SceneEventReceiver::UnRegister(entt::dispatcher& dispatcher)
{
dispatcher.sink<BeforeEnterScene>().disconnect<&SceneEventReceiver::SceneEventReceiver::Receive0>();
dispatcher.sink<OnEnterScene>().disconnect<&SceneEventReceiver::SceneEventReceiver::Receive1>();
dispatcher.sink<BeforeLeaveScene>().disconnect<&SceneEventReceiver::SceneEventReceiver::Receive2>();
dispatcher.sink<OnLeaveScene>().disconnect<&SceneEventReceiver::SceneEventReceiver::Receive3>();
}

void SceneEventReceiver::Receive0(const BeforeEnterScene& event_obj)
{
///<<< BEGIN WRITING YOUR CODE 
 
///<<< END WRITING YOUR CODE 
}

void SceneEventReceiver::Receive1(const OnEnterScene& event_obj)
{
///<<< BEGIN WRITING YOUR CODE 

///<<< END WRITING YOUR CODE 
}

void SceneEventReceiver::Receive2(const BeforeLeaveScene& event_obj)
{
///<<< BEGIN WRITING YOUR CODE 
   
///<<< END WRITING YOUR CODE 
}

void SceneEventReceiver::Receive3(const OnLeaveScene& event_obj)
{
///<<< BEGIN WRITING YOUR CODE 
///<<< END WRITING YOUR CODE 
}

