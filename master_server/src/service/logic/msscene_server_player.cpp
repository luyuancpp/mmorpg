#include "msscene_server_player.h"
#include "src/game_logic/game_registry.h"
#include "src/network/message_system.h"
///<<< BEGIN WRITING YOUR CODE

#include "muduo/base/Logging.h"

#include "src/game_logic/scene/scene.h"
#include "src/game_logic/scene/servernode_system.h"
#include "src/game_logic/tips_id.h"
#include "src/network/gs_node.h"
#include "src/network/player_session.h"
#include "src/system/player_scene_system.h"
#include "src/system/player_tip_system.h"

#include "component_proto/scene_comp.pb.h"
#include "gs_service.pb.h"

using GsStubPtr = std::unique_ptr<RpcStub<gsservice::GsService_Stub>>;
///<<< END WRITING YOUR CODE

///<<<rpc begin
void ServerPlayerSceneServiceImpl::EnterSceneGs2Ms(entt::entity player,
    const ::Gs2MsEnterSceneRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
    auto scene_id = request->scene_info().scene_id();
    entt::entity scene = entt::null;
    if (scene_id <= 0)//用scene_config id 去换本服的ms
    {
        GetSceneParam getp;
        getp.scene_confid_ = request->scene_info().scene_confid();
        scene = ServerNodeSystem::GetWeightRoundRobinMainScene(getp);
        if (entt::null == scene)
        {
            PlayerTipSystem::Tip(player, kRetEnterSceneSceneFull, {});
            return;
        }
        scene_id = registry.get<SceneInfo>(scene).scene_id();
    }
    else
    {
        scene = ScenesSystem::GetSingleton().get_scene(scene_id);
        if (entt::null == scene)
        {
            PlayerTipSystem::Tip(player, kRetEnterSceneSceneNotFound, {});
            return;
        }
    }

    //跨服场景，通知跨服去换
    if (registry.any_of<CrossMainSceneServer>(scene))
    {
        return;
    }

	//您当前就在这个场景，无需切换
	auto try_scene_entity = registry.try_get<SceneEntity>(player);
	if (nullptr != try_scene_entity)
	{
		if (scene != entt::null && scene == try_scene_entity->scene_entity())
		{
			PlayerTipSystem::Tip(player, kRetEnterSceneYouInCurrentScene, {});
			return;
		}
	}

    CheckEnterSceneParam check_enter_scene_p;
    check_enter_scene_p.scene_id_ = scene_id;
    check_enter_scene_p.player_ = player;
    auto ret = ScenesSystem::GetSingleton().CheckScenePlayerSize(check_enter_scene_p);
    if (kRetOK != ret)
    {
        PlayerTipSystem::Tip(player, ret, {});
        return;
    }
    PlayerSceneSystem::ChangeScene(player, scene);
///<<< END WRITING YOUR CODE
}

void ServerPlayerSceneServiceImpl::EnterSceneMs2Gs(entt::entity player,
    const ::Ms2GsEnterSceneRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ServerPlayerSceneServiceImpl::LeaveSceneGs2Ms(entt::entity player,
    const ::Gs2MsLeaveSceneRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ServerPlayerSceneServiceImpl::LeaveSceneMs2Gs(entt::entity player,
    const ::Ms2GsLeaveSceneRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ServerPlayerSceneServiceImpl::Gs2MsLeaveSceneAsyncSavePlayerComplete(entt::entity player,
    const ::Gs2MsLeaveSceneAsyncSavePlayerCompleteRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
    auto try_change_gs_enter_scene = registry.try_get<AfterChangeGsEnterScene>(player);
    if (nullptr == try_change_gs_enter_scene)
    {
        LOG_ERROR << "change gs scene compnent null" << registry.get<Guid>(player);
        return;
    }
	auto scene = ScenesSystem::GetSingleton().get_scene(try_change_gs_enter_scene->scene_info().scene_id());
    registry.remove<AfterChangeGsEnterScene>(player);
    //todo异步加载完场景已经不在了scene了
	if (entt::null == scene)
	{
		LOG_ERROR << "change gs scene scene not found or destroy" << registry.get<Guid>(player);
		return;
	}

	EnterSceneParam ep;
	ep.enterer_ = player;
	ep.scene_ = scene;
	ScenesSystem::GetSingleton().EnterScene(ep);

    auto try_player_session = registry.try_get<PlayerSession>(player);
    if (nullptr == try_player_session)
	{
		LOG_ERROR << "change gs scene scene not found or destroy" << registry.get<Guid>(player);
		return;

    }
    else
    {
        auto* p_gs_data = registry.try_get<GsNodePtr>(scene);
        if (nullptr == p_gs_data)//找不到gs了，放到好的gs里面
        {
            // todo default
            LOG_ERROR << "player " << registry.get<Guid>(player) << " enter default secne";
        }
        else
        {
            try_player_session->gs_ = *p_gs_data;
        }
    }
    PlayerSceneSystem::SendEnterGs(player);
///<<< END WRITING YOUR CODE
}

///<<<rpc end
