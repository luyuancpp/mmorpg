#include "scene_event_handler.h"
#include "event_proto/scene_event.pb.h"
#include "thread_local/storage.h"
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
void SceneEventHandler::Register()
{
		tls.dispatcher.sink<OnSceneCreate>().connect<&SceneEventHandler::OnSceneCreateHandler>();
		tls.dispatcher.sink<OnDestroyScene>().connect<&SceneEventHandler::OnDestroySceneHandler>();
		tls.dispatcher.sink<BeforeEnterScene>().connect<&SceneEventHandler::BeforeEnterSceneHandler>();
		tls.dispatcher.sink<AfterEnterScene>().connect<&SceneEventHandler::AfterEnterSceneHandler>();
		tls.dispatcher.sink<BeforeLeaveScene>().connect<&SceneEventHandler::BeforeLeaveSceneHandler>();
		tls.dispatcher.sink<AfterLeaveScene>().connect<&SceneEventHandler::AfterLeaveSceneHandler>();
		tls.dispatcher.sink<S2CEnterScene>().connect<&SceneEventHandler::S2CEnterSceneHandler>();
}

void SceneEventHandler::UnRegister()
{
		tls.dispatcher.sink<OnSceneCreate>().disconnect<&SceneEventHandler::OnSceneCreateHandler>();
		tls.dispatcher.sink<OnDestroyScene>().disconnect<&SceneEventHandler::OnDestroySceneHandler>();
		tls.dispatcher.sink<BeforeEnterScene>().disconnect<&SceneEventHandler::BeforeEnterSceneHandler>();
		tls.dispatcher.sink<AfterEnterScene>().disconnect<&SceneEventHandler::AfterEnterSceneHandler>();
		tls.dispatcher.sink<BeforeLeaveScene>().disconnect<&SceneEventHandler::BeforeLeaveSceneHandler>();
		tls.dispatcher.sink<AfterLeaveScene>().disconnect<&SceneEventHandler::AfterLeaveSceneHandler>();
		tls.dispatcher.sink<S2CEnterScene>().disconnect<&SceneEventHandler::S2CEnterSceneHandler>();
}

void SceneEventHandler::OnSceneCreateHandler(const OnSceneCreate& message)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void SceneEventHandler::OnDestroySceneHandler(const OnDestroyScene& message)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void SceneEventHandler::BeforeEnterSceneHandler(const BeforeEnterScene& message)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void SceneEventHandler::AfterEnterSceneHandler(const AfterEnterScene& message)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void SceneEventHandler::BeforeLeaveSceneHandler(const BeforeLeaveScene& message)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void SceneEventHandler::AfterLeaveSceneHandler(const AfterLeaveScene& message)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void SceneEventHandler::S2CEnterSceneHandler(const S2CEnterScene& message)
{
///<<< BEGIN WRITING YOUR CODE

///<<< END WRITING YOUR CODE
}

