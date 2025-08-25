#include "scene_event_handler.h"
#include "proto/logic/event/scene_event.pb.h"
#include "threading/dispatcher_manager.h"

///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"
#include "scene/system/aoi_system.h"
#include "scene/system/game_node_scene_system.h"
#include "scene/system/scene_crowd_system.h"
///<<< END WRITING YOUR CODE


void SceneEventHandler::Register()
{
    dispatcher.sink<OnSceneCreate>().connect<&SceneEventHandler::OnSceneCreateHandler>();
    dispatcher.sink<OnDestroyScene>().connect<&SceneEventHandler::OnDestroySceneHandler>();
    dispatcher.sink<BeforeEnterScene>().connect<&SceneEventHandler::BeforeEnterSceneHandler>();
    dispatcher.sink<AfterEnterScene>().connect<&SceneEventHandler::AfterEnterSceneHandler>();
    dispatcher.sink<BeforeLeaveScene>().connect<&SceneEventHandler::BeforeLeaveSceneHandler>();
    dispatcher.sink<AfterLeaveScene>().connect<&SceneEventHandler::AfterLeaveSceneHandler>();
    dispatcher.sink<S2CEnterScene>().connect<&SceneEventHandler::S2CEnterSceneHandler>();
}

void SceneEventHandler::UnRegister()
{
    dispatcher.sink<OnSceneCreate>().disconnect<&SceneEventHandler::OnSceneCreateHandler>();
    dispatcher.sink<OnDestroyScene>().disconnect<&SceneEventHandler::OnDestroySceneHandler>();
    dispatcher.sink<BeforeEnterScene>().disconnect<&SceneEventHandler::BeforeEnterSceneHandler>();
    dispatcher.sink<AfterEnterScene>().disconnect<&SceneEventHandler::AfterEnterSceneHandler>();
    dispatcher.sink<BeforeLeaveScene>().disconnect<&SceneEventHandler::BeforeLeaveSceneHandler>();
    dispatcher.sink<AfterLeaveScene>().disconnect<&SceneEventHandler::AfterLeaveSceneHandler>();
    dispatcher.sink<S2CEnterScene>().disconnect<&SceneEventHandler::S2CEnterSceneHandler>();
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
