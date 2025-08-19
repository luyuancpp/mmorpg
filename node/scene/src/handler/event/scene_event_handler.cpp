#include "scene_event_handler.h"
#include "proto/logic/event/scene_event.pb.h"
#include "thread_local/storage.h"

///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"
#include "scene/system/aoi_system.h"
#include "scene/system/game_node_scene_system.h"
#include "scene/system/scene_crowd_system.h"
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
void SceneEventHandler::OnSceneCreateHandler(const OnSceneCreate& event)
{
///<<< BEGIN WRITING YOUR CODE
	GameNodeSceneSystem::HandleSceneCreation(event);
///<<< END WRITING YOUR CODE
}
void SceneEventHandler::OnDestroySceneHandler(const OnDestroyScene& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
void SceneEventHandler::BeforeEnterSceneHandler(const BeforeEnterScene& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
void SceneEventHandler::AfterEnterSceneHandler(const AfterEnterScene& event)
{
///<<< BEGIN WRITING YOUR CODE
	//GameNodeSceneSystem::AfterEnterSceneHandler(message);
	SceneCrowdSystem::AfterEnterSceneHandler(event);
///<<< END WRITING YOUR CODE
}
void SceneEventHandler::BeforeLeaveSceneHandler(const BeforeLeaveScene& event)
{
///<<< BEGIN WRITING YOUR CODE
	//GameNodeSceneSystem::BeforeLeaveSceneHandler(message);
	SceneCrowdSystem::BeforeLeaveSceneHandler(event);
	AoiSystem::BeforeLeaveSceneHandler(event);
///<<< END WRITING YOUR CODE
}
void SceneEventHandler::AfterLeaveSceneHandler(const AfterLeaveScene& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
void SceneEventHandler::S2CEnterSceneHandler(const S2CEnterScene& event)
{
///<<< BEGIN WRITING YOUR CODE

///<<< END WRITING YOUR CODE
}
