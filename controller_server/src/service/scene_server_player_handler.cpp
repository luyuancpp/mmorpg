#include "scene_server_player_handler.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/network/message_system.h"
///<<< BEGIN WRITING YOUR CODE

#include "muduo/base/Logging.h"

#include "src/comp/player_list.h"
#include "src/game_logic/scene/scene.h"
#include "src/game_logic/scene/servernode_system.h"
#include "src/game_logic/tips_id.h"
#include "src/network/gs_node.h"
#include "src/network/player_session.h"
#include "src/controller_server.h"
#include "src/system/player_scene_system.h"
#include "src/system/player_tip_system.h"
#include "src/system/player_change_scene.h"

#include "game_service.pb.h"

///<<< END WRITING YOUR CODE

///<<<rpc begin
void ServerPlayerSceneServiceHandler::EnterSceneGs2Controller(entt::entity player,
    const ::Gs2ControllerEnterSceneRequest* request,
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

void ServerPlayerSceneServiceHandler::EnterSceneController2Gs(entt::entity player,
    const ::Controller2GsEnterSceneRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ServerPlayerSceneServiceHandler::LeaveSceneGs2Controller(entt::entity player,
    const ::Gs2ControllerLeaveSceneRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ServerPlayerSceneServiceHandler::LeaveSceneController2Gs(entt::entity player,
    const ::Controller2GsLeaveSceneRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ServerPlayerSceneServiceHandler::Gs2ControllerLeaveSceneAsyncSavePlayerComplete(entt::entity player,
    const ::Gs2ControllerLeaveSceneAsyncSavePlayerCompleteRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
    //异步切换考虑消息队列
	GetPlayerCompnentMemberReturnVoid(change_scene_queue, PlayerControllerChangeSceneQueue);
	if (change_scene_queue.empty())
	{
		return;
	}
	auto& change_scene_info = change_scene_queue.front();
	LOG_DEBUG << "Gs2ControllerLeaveSceneAsyncSavePlayerComplete  change scene " << change_scene_info.processing();
	auto to_scene = ScenesSystem::get_scene(change_scene_info.scene_info().scene_id());
    //todo异步加载完场景已经不在了scene了

	if (entt::null == to_scene)//todo 场景崩溃了要去新的场景
	{
		LOG_ERROR << "change gs scene scene not found or destroy" << tls.registry.get<Guid>(player);
		return;
	}
    auto try_player_session = tls.registry.try_get<PlayerSession>(player);
    if (nullptr == try_player_session)
	{
		LOG_ERROR << "change gs scene scene not found or destroy" << tls.registry.get<Guid>(player);
		PlayerChangeSceneSystem::PopFrontChangeSceneQueue(player);
		return;

    }
    
	GsNodePtr null_gs;
	try_player_session->set_gs(null_gs);

    PlayerSceneSystem::CallPlayerEnterGs(player, PlayerSceneSystem::GetGsNodeIdByScene(to_scene), try_player_session->session_id());
///<<< END WRITING YOUR CODE
}

void ServerPlayerSceneServiceHandler::Controller2GsEnterSceneS2C(entt::entity player,
    const ::EnterSceneS2CRequest* request,
    ::EnterScenerS2CResponse* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

///<<<rpc end
