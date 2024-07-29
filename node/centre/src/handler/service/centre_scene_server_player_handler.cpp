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
	CentreChangeSceneInfo changeSceneInfo;
	PlayerChangeSceneSystem::CopySceneInfoToChangeInfo(changeSceneInfo, request->scene_info());
	if (const auto ret = PlayerChangeSceneSystem::PushChangeSceneInfo(player, changeSceneInfo); ret != kOK)
	{
		PlayerTipSystem::SendToPlayer(player, ret, {});
		return;
	}
	PlayerSceneSystem::AttemptEnterNextScene(player);
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
	auto* const changeSceneQueue = tls.registry.try_get<PlayerCentreChangeSceneQueue>(player);
	if (!changeSceneQueue || changeSceneQueue->changeSceneQueue.empty())
	{
		return;
	}
	const auto& changeSceneInfo = changeSceneQueue->changeSceneQueue.front();
	LOG_DEBUG << "Gs2CentreLeaveSceneAsyncSavePlayerComplete  change scene " << changeSceneInfo.processing();
	const auto toScene = entt::to_entity(changeSceneInfo.guid());
	//todo异步加载完场景已经不在了scene了
	//todo 场景崩溃了要去新的场景
	if (entt::null == toScene)
	{
		LOG_ERROR << "change gs scene scene not found or destroy" << tls.registry.get<Guid>(player);
		return;
	}
	auto* const playerNodeInfo = tls.registry.try_get<PlayerNodeInfo>(player);
	if (!playerNodeInfo)
	{
		//todo 
		LOG_ERROR << "change gs scene scene not found or destroy" << tls.registry.get<Guid>(player);
		PlayerChangeSceneSystem::PopFrontChangeSceneQueue(player);
		return;
	}
	playerNodeInfo->set_game_node_id(kInvalidNodeId);

	PlayerSceneSystem::ProcessPlayerEnterGameServer(player, SceneSystem::GetGameNodeId(toScene));
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
	SendToPlayer(ClientPlayerSceneServicePushSceneInfoS2CMsgId, message, player);
///<<< END WRITING YOUR CODE
}
