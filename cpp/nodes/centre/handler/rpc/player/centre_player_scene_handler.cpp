
#include "centre_player_scene_handler.h"

///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"

#include "table/proto/tip/scene_error_tip.pb.h"
#include "table/proto/tip/common_error_tip.pb.h"
#include "centre_node.h"
#include "scene/system/player_room.h"
#include "player/system/player_tip.h"
#include "scene/system/player_change_room.h"
#include "rpc/service_metadata/player_scene_service_metadata.h"
#include "network/player_message_utils.h"
#include "proto/logic/component/player_network_comp.pb.h"
#include "node/system/node/node_util.h"
#include "threading/redis_manager.h"
#include "engine/core/node/system/zone_utils.h"
#include <modules/scene/system/room_common.h>
///<<< END WRITING YOUR CODE



void CentrePlayerSceneHandler::EnterScene(entt::entity player,const ::CentreEnterSceneRequest* request,
	::google::protobuf::Empty* response)
{
	///<<< BEGIN WRITING YOUR CODE
	//正在切换场景中，不能马上切换，gs崩溃了怎么办

	// Handler 只负责参数校验和调用
	if (!request->has_scene_info()) {
		LOG_ERROR << "Scene info missing";
		return;
	}
	PlayerSceneSystem::HandleEnterScene(player, request->scene_info());

	///<<< END WRITING YOUR CODE

}


void CentrePlayerSceneHandler::LeaveScene(entt::entity player,const ::CentreLeaveSceneRequest* request,
	::google::protobuf::Empty* response)
{
	///<<< BEGIN WRITING YOUR CODE
	///<<< END WRITING YOUR CODE

}


void CentrePlayerSceneHandler::LeaveSceneAsyncSavePlayerComplete(entt::entity player,const ::CentreLeaveSceneAsyncSavePlayerCompleteRequest* request,
	::google::protobuf::Empty* response)
{
	///<<< BEGIN WRITING YOUR CODE
		//异步切换考虑消息队列
		//todo异步加载完场景已经不在了scene了
		//todo 场景崩溃了要去新的场景

	auto* const changeSceneQueue = tlsRegistryManager.actorRegistry.try_get<ChangeSceneQueuePBComponent>(player);
	if (!changeSceneQueue || changeSceneQueue->empty()) {
		LOG_WARN << " Change scene queue is empty, player: " << tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(player);
		// 可选：通知客户端或重试
		return;
	}


	const auto& changeSceneInfo = *changeSceneQueue->front();
	const auto toScene = entt::to_entity(changeSceneInfo.guid());
	if (entt::null == toScene)
	{
		LOG_ERROR << "Destination scene not found or destroyed for player: " << tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(player);
		return;
	}

	auto* const playerSessionSnapshotPB = tlsRegistryManager.actorRegistry.try_get<PlayerSessionSnapshotPBComp>(player);
	if (!playerSessionSnapshotPB)
	{
		LOG_ERROR << "PlayerNodeInfo not found for player: " << tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(player);
		PlayerChangeRoomUtil::PopFrontChangeSceneQueue(player);
		return;
	}

	playerSessionSnapshotPB->mutable_node_id()->erase(eNodeType::SceneNodeService);
	PlayerSceneSystem::ProcessPlayerEnterSceneNode(player, RoomCommon::GetGameNodeIdFromGuid(toScene));
	LOG_INFO << "LeaveSceneAsyncSavePlayerComplete request processed successfully for player: " << tlsRegistryManager.actorRegistry.get_or_emplace<Guid>(player);
	///<<< END WRITING YOUR CODE

}


void CentrePlayerSceneHandler::SceneInfoC2S(entt::entity player,const ::CentreSceneInfoRequest* request,
	::google::protobuf::Empty* response)
{
	///<<< BEGIN WRITING YOUR CODE
		//给客户端发所有场景消息
	SceneInfoS2C message;
	for (const auto& [entity, info] : tlsRegistryManager.roomRegistry.view<RoomInfoPBComponent>().each())
	{
		message.mutable_scene_info()->Add()->CopyFrom(info);
	}

	SendMessageToClientViaGate(SceneSceneClientPlayerNotifySceneInfoMessageId, message, player);
	///<<< END WRITING YOUR CODE

}

