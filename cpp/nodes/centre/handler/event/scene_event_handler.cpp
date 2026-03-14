#include "scene_event_handler.h"
#include "threading/dispatcher_manager.h"

///<<< BEGIN WRITING YOUR CODE 

#include "proto/common/component/player_network_comp.pb.h"
#include "muduo/base/Logging.h"

#include "modules/scene/comp/scene_comp.h"
#include "rpc/service_metadata/game_player_scene_service_metadata.h"
#include "scene/system/player_change_scene.h"
#include "scene/system/player_scene.h"
#include "engine/core/type_define/type_define.h"
#include "rpc/service_metadata/game_player_service_metadata.h"
#include "proto/common/component/scene_comp.pb.h"
#include "network/player_message_utils.h"
#include <threading/registry_manager.h>
#include "proto/common/base/common.pb.h"

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

///<<< END WRITING YOUR CODE
}
void SceneEventHandler::BeforeLeaveSceneHandler(const BeforeLeaveScene& event)
{
	///<<< BEGIN WRITING YOUR CODE
		const auto player = entt::to_entity(event.entity());

	const auto& changeSceneQueue = tlsRegistryManager.actorRegistry.get_or_emplace<ChangeSceneQueuePBComponent>(player);

	GsLeaveSceneRequest leaveSceneRequest;

	if (!changeSceneQueue.empty())
	{
		const auto& changeSceneInfo = *changeSceneQueue.front();
		*leaveSceneRequest.mutable_change_scene_info() = changeSceneInfo;
	}

	SendMessageToPlayerOnSceneNode(SceneScenePlayerLeaveSceneMessageId, leaveSceneRequest, player);

	LOG_INFO << "Player is leaving scene "
		<< tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(player)
		<< ", Scene GUID: "
		<< tlsRegistryManager.sceneRegistry.get<SceneInfoPBComponent>(tlsRegistryManager.actorRegistry.get_or_emplace<SceneEntityComp>(player).sceneEntity).guid();

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
