#include "scene_event_handler.h"
#include "proto/common/event/scene_event.pb.h"
#include "threading/dispatcher_manager.h"

///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"
#include "scene/scene/system/aoi.h"
#include "scene/scene/system/game_node_scene.h"
#include "scene/scene/system/scene_crowd.h"
///<<< END WRITING YOUR CODE


void SceneEventHandler::Register()
{
    dispatcher.sink<OnSceneCreated>().connect<&SceneEventHandler::OnSceneCreatedHandler>();
    dispatcher.sink<OnSceneDestroyed>().connect<&SceneEventHandler::OnSceneDestroyedHandler>();
    dispatcher.sink<BeforeEnterScene>().connect<&SceneEventHandler::BeforeEnterSceneHandler>();
    dispatcher.sink<AfterEnterScene>().connect<&SceneEventHandler::AfterEnterSceneHandler>();
    dispatcher.sink<BeforeLeaveScene>().connect<&SceneEventHandler::BeforeLeaveSceneHandler>();
    dispatcher.sink<AfterLeaveScene>().connect<&SceneEventHandler::AfterLeaveSceneHandler>();
    dispatcher.sink<S2CEnterScene>().connect<&SceneEventHandler::S2CEnterSceneHandler>();
}

void SceneEventHandler::UnRegister()
{
    dispatcher.sink<OnSceneCreated>().disconnect<&SceneEventHandler::OnSceneCreatedHandler>();
    dispatcher.sink<OnSceneDestroyed>().disconnect<&SceneEventHandler::OnSceneDestroyedHandler>();
    dispatcher.sink<BeforeEnterScene>().disconnect<&SceneEventHandler::BeforeEnterSceneHandler>();
    dispatcher.sink<AfterEnterScene>().disconnect<&SceneEventHandler::AfterEnterSceneHandler>();
    dispatcher.sink<BeforeLeaveScene>().disconnect<&SceneEventHandler::BeforeLeaveSceneHandler>();
    dispatcher.sink<AfterLeaveScene>().disconnect<&SceneEventHandler::AfterLeaveSceneHandler>();
    dispatcher.sink<S2CEnterScene>().disconnect<&SceneEventHandler::S2CEnterSceneHandler>();
}
void SceneEventHandler::OnSceneCreatedHandler(const OnSceneCreated& event)
{
///<<< BEGIN WRITING YOUR CODE
   	GameNodeSceneSystem::HandleSceneCreation(event);
///<<< END WRITING YOUR CODE
}
void SceneEventHandler::OnSceneDestroyedHandler(const OnSceneDestroyed& event)
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
