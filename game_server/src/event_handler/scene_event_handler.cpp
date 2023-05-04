#include "scene_event_handler.h"
#include "event_proto/scene_event.pb.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
void SceneEventHandler::Register(entt::dispatcher& dispatcher)
{
dispatcher.sink<BeforeEnterScene>().connect<&SceneEventHandler::SceneEventHandler::Receive0>();
dispatcher.sink<OnEnterScene>().connect<&SceneEventHandler::SceneEventHandler::Receive1>();
dispatcher.sink<BeforeLeaveScene>().connect<&SceneEventHandler::SceneEventHandler::Receive2>();
dispatcher.sink<OnLeaveScene>().connect<&SceneEventHandler::SceneEventHandler::Receive3>();
dispatcher.sink<S2CEnterScene>().connect<&SceneEventHandler::SceneEventHandler::Receive4>();
}

void SceneEventHandler::UnRegister(entt::dispatcher& dispatcher)
{
dispatcher.sink<BeforeEnterScene>().disconnect<&SceneEventHandler::SceneEventHandler::Receive0>();
dispatcher.sink<OnEnterScene>().disconnect<&SceneEventHandler::SceneEventHandler::Receive1>();
dispatcher.sink<BeforeLeaveScene>().disconnect<&SceneEventHandler::SceneEventHandler::Receive2>();
dispatcher.sink<OnLeaveScene>().disconnect<&SceneEventHandler::SceneEventHandler::Receive3>();
dispatcher.sink<S2CEnterScene>().disconnect<&SceneEventHandler::SceneEventHandler::Receive4>();
}

void SceneEventHandler::Receive0(const BeforeEnterScene& event_obj)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void SceneEventHandler::Receive1(const OnEnterScene& event_obj)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void SceneEventHandler::Receive2(const BeforeLeaveScene& event_obj)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void SceneEventHandler::Receive3(const OnLeaveScene& event_obj)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void SceneEventHandler::Receive4(const S2CEnterScene& event_obj)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

