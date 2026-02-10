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
    dispatcher.sink<OnRoomCreated>().connect<&SceneEventHandler::OnRoomCreatedHandler>();
    dispatcher.sink<OnRoomDestroyed>().connect<&SceneEventHandler::OnRoomDestroyedHandler>();
    dispatcher.sink<BeforeEnterRoom>().connect<&SceneEventHandler::BeforeEnterRoomHandler>();
    dispatcher.sink<AfterEnterRoom>().connect<&SceneEventHandler::AfterEnterRoomHandler>();
    dispatcher.sink<BeforeLeaveRoom>().connect<&SceneEventHandler::BeforeLeaveRoomHandler>();
    dispatcher.sink<AfterLeaveRoom>().connect<&SceneEventHandler::AfterLeaveRoomHandler>();
    dispatcher.sink<S2CEnterRoom>().connect<&SceneEventHandler::S2CEnterRoomHandler>();
}

void SceneEventHandler::UnRegister()
{
    dispatcher.sink<OnRoomCreated>().disconnect<&SceneEventHandler::OnRoomCreatedHandler>();
    dispatcher.sink<OnRoomDestroyed>().disconnect<&SceneEventHandler::OnRoomDestroyedHandler>();
    dispatcher.sink<BeforeEnterRoom>().disconnect<&SceneEventHandler::BeforeEnterRoomHandler>();
    dispatcher.sink<AfterEnterRoom>().disconnect<&SceneEventHandler::AfterEnterRoomHandler>();
    dispatcher.sink<BeforeLeaveRoom>().disconnect<&SceneEventHandler::BeforeLeaveRoomHandler>();
    dispatcher.sink<AfterLeaveRoom>().disconnect<&SceneEventHandler::AfterLeaveRoomHandler>();
    dispatcher.sink<S2CEnterRoom>().disconnect<&SceneEventHandler::S2CEnterRoomHandler>();
}
void SceneEventHandler::OnRoomCreatedHandler(const OnRoomCreated& event)
{
///<<< BEGIN WRITING YOUR CODE
   	GameNodeSceneSystem::HandleSceneCreation(event);
///<<< END WRITING YOUR CODE
}
void SceneEventHandler::OnRoomDestroyedHandler(const OnRoomDestroyed& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
void SceneEventHandler::BeforeEnterRoomHandler(const BeforeEnterRoom& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
void SceneEventHandler::AfterEnterRoomHandler(const AfterEnterRoom& event)
{
///<<< BEGIN WRITING YOUR CODE
	//GameNodeSceneSystem::AfterEnterSceneHandler(message);
	SceneCrowdSystem::AfterEnterSceneHandler(event);
///<<< END WRITING YOUR CODE
}
void SceneEventHandler::BeforeLeaveRoomHandler(const BeforeLeaveRoom& event)
{
///<<< BEGIN WRITING YOUR CODE
	//GameNodeSceneSystem::BeforeLeaveSceneHandler(message);
	SceneCrowdSystem::BeforeLeaveSceneHandler(event);
	AoiSystem::BeforeLeaveSceneHandler(event);
///<<< END WRITING YOUR CODE
}
void SceneEventHandler::AfterLeaveRoomHandler(const AfterLeaveRoom& event)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}
void SceneEventHandler::S2CEnterRoomHandler(const S2CEnterRoom& event)
{
///<<< BEGIN WRITING YOUR CODE

///<<< END WRITING YOUR CODE
}
