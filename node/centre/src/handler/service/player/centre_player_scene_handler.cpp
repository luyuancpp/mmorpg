#include "centre_player_scene_handler.h"

///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"

#include "scene/system/scene_system.h"
#include "pbc/scene_error_tip.pb.h"
#include "pbc/common_error_tip.pb.h"
#include "centre_node.h"
#include "scene/system/player_scene_system.h"
#include "player/system/player_tip_system.h"
#include "scene/system/player_change_scene_system.h"
#include "network/message_system.h"
#include "service_info/player_scene_service_info.h"

#include "proto/logic/component/player_network_comp.pb.h"
///<<< END WRITING YOUR CODE



void CentrePlayerSceneHandler::EnterScene(entt::entity player, const ::CentreEnterSceneRequest* request,
	::google::protobuf::Empty* response)
{
	///<<< BEGIN WRITING YOUR CODE
		//正在切换场景中，不能马上切换，gs崩溃了怎么办
	LOG_INFO << "EnterScene request received for player: " << tls.registry.get<Guid>(player)
		<< ", scene_info: " << request->scene_info().DebugString();

	ChangeSceneInfoPBComponent changeSceneInfo;
	PlayerChangeSceneUtil::CopySceneInfoToChangeInfo(changeSceneInfo, request->scene_info());
	if (const auto ret = PlayerChangeSceneUtil::PushChangeSceneInfo(player, changeSceneInfo); ret != kSuccess)
	{
		LOG_ERROR << "Failed to push change scene info for player " << tls.registry.get<Guid>(player) << ": " << ret;
		PlayerTipSystem::SendToPlayer(player, ret, {});
		return;
	}

	PlayerSceneSystem::AttemptEnterNextScene(player);

	LOG_INFO << "EnterScene request processed successfully for player: " << tls.registry.get<Guid>(player);
	///<<< END WRITING YOUR CODE


}



void CentrePlayerSceneHandler::LeaveScene(entt::entity player, const ::CentreLeaveSceneRequest* request,
	::google::protobuf::Empty* response)
{
	///<<< BEGIN WRITING YOUR CODE
	///<<< END WRITING YOUR CODE


}



void CentrePlayerSceneHandler::LeaveSceneAsyncSavePlayerComplete(entt::entity player, const ::CentreLeaveSceneAsyncSavePlayerCompleteRequest* request,
	::google::protobuf::Empty* response)
{
	///<<< BEGIN WRITING YOUR CODE
		//异步切换考虑消息队列
		//todo异步加载完场景已经不在了scene了
		//todo 场景崩溃了要去新的场景
	LOG_INFO << "LeaveSceneAsyncSavePlayerComplete request received for player: " << tls.registry.get<Guid>(player);

	auto* const changeSceneQueue = tls.registry.try_get<ChangeSceneQueuePBComponent>(player);
	if (!changeSceneQueue || changeSceneQueue->changeSceneQueue.empty())
	{
		LOG_WARN << "Change scene queue is empty for player: " << tls.registry.get<Guid>(player);
		return;
	}

	const auto& changeSceneInfo = changeSceneQueue->changeSceneQueue.front();
	LOG_INFO << "Processing change scene info: " << changeSceneInfo.processing();

	const auto toScene = entt::to_entity(changeSceneInfo.guid());
	if (entt::null == toScene)
	{
		LOG_ERROR << "Destination scene not found or destroyed for player: " << tls.registry.get<Guid>(player);
		return;
	}

	auto* const playerNodeInfo = tls.registry.try_get<PlayerNodeInfoPBComponent>(player);
	if (!playerNodeInfo)
	{
		LOG_ERROR << "PlayerNodeInfo not found for player: " << tls.registry.get<Guid>(player);
		PlayerChangeSceneUtil::PopFrontChangeSceneQueue(player);
		return;
	}

	playerNodeInfo->set_scene_node_id(kInvalidNodeId);

	PlayerSceneSystem::ProcessPlayerEnterSceneNode(player, SceneUtil::GetGameNodeId(toScene));

	LOG_INFO << "LeaveSceneAsyncSavePlayerComplete request processed successfully for player: " << tls.registry.get<Guid>(player);
	///<<< END WRITING YOUR CODE


}



void CentrePlayerSceneHandler::SceneInfoC2S(entt::entity player, const ::CentreSceneInfoRequest* request,
	::google::protobuf::Empty* response)
{
	///<<< BEGIN WRITING YOUR CODE
		//给客户端发所有场景消息
	SceneInfoS2C message;
	for (const auto& [entity, info] : tls.sceneRegistry.view<SceneInfoPBComponent>().each())
	{
		message.mutable_scene_info()->Add()->CopyFrom(info);
	}

	SendMessageToPlayer(SceneSceneClientPlayerNotifySceneInfoMessageId, message, player);
	///<<< END WRITING YOUR CODE


}


