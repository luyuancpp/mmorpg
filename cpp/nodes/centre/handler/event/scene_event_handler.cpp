#include "scene_event_handler.h"
#include "proto/logic/event/scene_event.pb.h"
#include "threading/dispatcher_manager.h"

///<<< BEGIN WRITING YOUR CODE 

#include "proto/logic/component/player_network_comp.pb.h"
#include "muduo/base/Logging.h"

#include "modules/scene/comp/scene_comp.h"
#include "rpc/service_metadata/game_player_scene_service_metadata.h"
#include "scene/system/player_change_room.h"
#include "scene/system/player_room.h"
#include "engine/core/type_define/type_define.h"
#include "rpc/service_metadata/game_player_service_metadata.h"
#include "proto/logic/component/scene_comp.pb.h"
#include "network/player_message_utils.h"
#include <threading/registry_manager.h>
#include "proto/common/common.pb.h"

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
  
///<<< END WRITING YOUR CODE
}
void SceneEventHandler::BeforeLeaveRoomHandler(const BeforeLeaveRoom& event)
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
		<< tlsRegistryManager.roomRegistry.get<RoomInfoPBComponent>(tlsRegistryManager.actorRegistry.get_or_emplace<RoomEntityComp>(player).roomEntity).guid();

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
    PlayerSceneSystem::SendToGameNodeEnterScene(entt::to_entity(event.entity()));
///<<< END WRITING YOUR CODE
}
