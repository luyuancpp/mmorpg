#include "scene_event_handler.h"
#include "proto/logic/event/scene_event.pb.h"
#include "thread_local/storage.h"

///<<< BEGIN WRITING YOUR CODE 

#include "proto/logic/component/player_network_comp.pb.h"
#include "muduo/base/Logging.h"

#include "scene/comp/scene_comp.h"
#include "network/message_system.h"
#include "service_info/game_player_scene_service_info.h"
#include "scene/system/player_change_scene_system.h"
#include "scene/system/player_scene_system.h"
#include "scene/system/scene_system.h"
#include "type_define/type_define.h"
#include "service_info/game_player_service_info.h"
#include "proto/logic/component/scene_comp.pb.h"

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
  
///<<< END WRITING YOUR CODE

}

void SceneEventHandler::BeforeLeaveSceneHandler(const BeforeLeaveScene& event)
{
	///<<< BEGIN WRITING YOUR CODE
	const auto player = entt::to_entity(event.entity());

	// Try to get the change scene queue component for the player
	auto* const changeSceneQueue = tls.registry.try_get<ChangeSceneQueuePBComponent>(player);

	// If the change scene queue component is not found, the queue is empty, or the scene change type is 'DifferentGs'
	if (!changeSceneQueue ||
		changeSceneQueue->changeSceneQueue.empty())
	{
		// 处理玩家直接退出游戏的情况
		// Handle the case where the player is exiting the game
		GameNodeExitGameRequest exitGameRequest;
		// Set any required fields for the exit game request here if needed
		SendToGsPlayer(ScenePlayerExitGameMessageId, exitGameRequest, player);

		LOG_TRACE << "Player is exiting the game: "
			<< tls.registry.get<Guid>(player);
		return;
	}

	const auto& changeSceneInfo = changeSceneQueue->changeSceneQueue.front();

	GsLeaveSceneRequest leaveSceneRequest;
	leaveSceneRequest.set_change_gs(changeSceneInfo.change_gs_type() == ChangeSceneInfoPBComponent::eDifferentGs);
	SendToGsPlayer(SceneScenePlayerLeaveSceneMessageId, leaveSceneRequest, player);

	LOG_TRACE << "Player is leaving scene "
		<< tls.registry.get<Guid>(player)
		<< ", Scene GUID: "
		<< tls.sceneRegistry.get<SceneInfoPBComponent>(tls.registry.get<SceneEntityComp>(player).sceneEntity).guid();
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
    PlayerSceneSystem::SendToGameNodeEnterScene(entt::to_entity(event.entity()));
///<<< END WRITING YOUR CODE

}
