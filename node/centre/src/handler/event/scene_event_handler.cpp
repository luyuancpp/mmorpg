#include "scene_event_handler.h"
#include "proto/logic/event/scene_event.pb.h"
#include "thread_local/storage.h"

///<<< BEGIN WRITING YOUR CODE 

#include "proto/logic/component/player_network_comp.pb.h"
#include "muduo/base/Logging.h"

#include "scene/comp/scene_comp.h"
#include "service_info/game_player_scene_service_info.h"
#include "scene/system/player_change_scene_system.h"
#include "scene/system/player_scene_system.h"
#include "scene/system/scene_system.h"
#include "type_define/type_define.h"
#include "service_info/game_player_service_info.h"
#include "proto/logic/component/scene_comp.pb.h"
#include "network/player_message_utils.h"

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

	auto* const changeSceneQueue = tls.actorRegistry.try_get<ChangeSceneQueuePBComponent>(player);

	GsLeaveSceneRequest leaveSceneRequest;

	if (changeSceneQueue && !changeSceneQueue->empty())
	{
		const auto& changeSceneInfo = *changeSceneQueue->front();
		*leaveSceneRequest.mutable_change_scene_info() = changeSceneInfo;
	}
	
	SendMessageToPlayerOnSceneNode(SceneScenePlayerLeaveSceneMessageId, leaveSceneRequest, player);

	LOG_INFO << "Player is leaving scene "
		<< tls.actorRegistry.get<Guid>(player)
		<< ", Scene GUID: "
		<< tls.sceneRegistry.get<SceneInfoPBComponent>(tls.actorRegistry.get<SceneEntityComp>(player).sceneEntity).guid();

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
