#include "centre_scene_server_player_handler.h"
#include "src/thread_local/thread_local_storage.h"
#include "src/network/message_system.h"
///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"

#include "src/system/scene/scene_system.h"
#include "src/constants/tips_id.h"
#include "src/network/game_node.h"
#include "src/centre_node.h"
#include "src/system/player_scene_system.h"
#include "src/system/player_tip_system.h"
#include "src/system/player_change_scene.h"
#include "component_proto/player_network_comp.pb.h"
///<<< END WRITING YOUR CODE
void CentreScenePlayerServiceHandler::EnterScene(entt::entity player,
	const ::CentreEnterSceneRequest* request,
	::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
//正在切换场景中，不能马上切换，gs崩溃了怎么办
	ControllerChangeSceneInfo change_scene_info;
	change_scene_info.mutable_scene_info()->CopyFrom(request->scene_info());
	auto ret = PlayerChangeSceneSystem::PushChangeSceneInfo(player, change_scene_info);
	if (ret != kRetOK)
	{
		PlayerTipSystem::Tip(player, ret, {});
		return;
	}
	PlayerSceneSystem::TryEnterNextScene(player);
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
	auto* const change_scene_queue = tls.registry.try_get<PlayerCentreChangeSceneQueue>(player);
	if (nullptr == change_scene_queue)
	{
		return;
	}
	if (change_scene_queue->change_scene_queue_.empty())
	{
		return;
	}
	const auto& change_scene_info = change_scene_queue->change_scene_queue_.front();
	LOG_DEBUG << "Gs2CentreLeaveSceneAsyncSavePlayerComplete  change scene " << change_scene_info.processing();
	const auto to_scene = entt::to_entity(change_scene_info.scene_info().guid());
	//todo异步加载完场景已经不在了scene了
	//todo 场景崩溃了要去新的场景
	if (entt::null == to_scene)
	{
		LOG_ERROR << "change gs scene scene not found or destroy" << tls.registry.get<Guid>(player);
		return;
	}
	auto* const player_node_info = tls.registry.try_get<PlayerNodeInfo>(player);
	if (nullptr == player_node_info)
	{
		LOG_ERROR << "change gs scene scene not found or destroy" << tls.registry.get<Guid>(player);
		PlayerChangeSceneSystem::PopFrontChangeSceneQueue(player);
		return;
	}
	player_node_info->set_game_node_id(kInvalidNodeId);

	PlayerSceneSystem::CallPlayerEnterGs(player, ScenesSystem::get_game_node_id(to_scene), player_node_info->gate_session_id());
///<<< END WRITING YOUR CODE
}

