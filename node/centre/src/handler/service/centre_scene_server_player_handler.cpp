#include "centre_scene_server_player_handler.h"
#include "thread_local/storage.h"
#include "system/network/message_system.h"
///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"

#include "system/scene/scene_system.h"
#include "constants/tips_id.h"
#include "comp/game_node.h"
#include "centre_node.h"
#include "system/player_scene.h"
#include "system/player_tip.h"
#include "system/player_change_scene.h"
#include "service/scene_client_player_service.h"

#include "proto/logic/component/player_network_comp.pb.h"
///<<< END WRITING YOUR CODE
void CentreScenePlayerServiceHandler::EnterScene(entt::entity player,
	const ::CentreEnterSceneRequest* request,
	::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
	//正在切换场景中，不能马上切换，gs崩溃了怎么办
	LOG_INFO << "EnterScene request received for player: " << tls.registry.get<Guid>(player)
		<< ", scene_info: " << request->scene_info().DebugString();

	CentreChangeSceneInfo changeSceneInfo;
	PlayerChangeSceneSystem::CopySceneInfoToChangeInfo(changeSceneInfo, request->scene_info());
	if (const auto ret = PlayerChangeSceneSystem::PushChangeSceneInfo(player, changeSceneInfo); ret != kOK)
	{
		LOG_ERROR << "Failed to push change scene info for player " << tls.registry.get<Guid>(player) << ": " << ret;
		PlayerTipSystem::SendToPlayer(player, ret, {});
		return;
	}

	PlayerSceneSystem::AttemptEnterNextScene(player);

	LOG_INFO << "EnterScene request processed successfully for player: " << tls.registry.get<Guid>(player);
///<<< END WRITING YOUR CODE
}

void CentreScenePlayerServiceHandler::LeaveScene(entt::entity player,
	const ::CentreLeaveSceneRequest* request,
	::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void CentreScenePlayerServiceHandler::LeaveSceneAsyncSavePlayerComplete(entt::entity player,
	const ::CentreLeaveSceneAsyncSavePlayerCompleteRequest* request,
	::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
	//异步切换考虑消息队列
	//todo异步加载完场景已经不在了scene了
	//todo 场景崩溃了要去新的场景
	LOG_INFO << "LeaveSceneAsyncSavePlayerComplete request received for player: " << tls.registry.get<Guid>(player);

	auto* const changeSceneQueue = tls.registry.try_get<PlayerCentreChangeSceneQueueComp>(player);
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

	auto* const playerNodeInfo = tls.registry.try_get<PlayerNodeInfo>(player);
	if (!playerNodeInfo)
	{
		LOG_ERROR << "PlayerNodeInfo not found for player: " << tls.registry.get<Guid>(player);
		PlayerChangeSceneSystem::PopFrontChangeSceneQueue(player);
		return;
	}

	playerNodeInfo->set_game_node_id(kInvalidNodeId);

	PlayerSceneSystem::ProcessPlayerEnterGameServer(player, SceneSystem::GetGameNodeId(toScene));

	LOG_INFO << "LeaveSceneAsyncSavePlayerComplete request processed successfully for player: " << tls.registry.get<Guid>(player);
///<<< END WRITING YOUR CODE
}

void CentreScenePlayerServiceHandler::SceneInfoC2S(entt::entity player,
	const ::SceneInfoRequest* request,
	::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
	//给客户端发所有场景消息
	SceneInfoS2C message;
	for (const auto& [entity, info] : tls.sceneRegistry.view<SceneInfo>().each())
	{
		message.mutable_scene_info()->Add()->CopyFrom(info);
	}

	SendMessageToPlayer(ClientPlayerSceneServicePushSceneInfoS2CMsgId, message, player);
///<<< END WRITING YOUR CODE
}

