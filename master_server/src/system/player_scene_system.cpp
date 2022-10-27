#include "player_scene_system.h"

#include "muduo/base/Logging.h"

#include "src/game_logic/comp/scene_comp.h"
#include "src/game_logic/scene/scene.h"
#include "src/game_logic/scene/servernode_system.h"
#include "src/game_logic/tips_id.h"
#include "src/network/message_system.h"
#include "src/network/player_session.h"

#include "component_proto/player_login_comp.pb.h"
#include "logic_proto/scene_server_player.pb.h"
#include "gs_service.pb.h"

using GsStubPtr = std::unique_ptr<RpcStub<gsservice::GsService_Stub>>;

void PlayerSceneSystem::Send2GsEnterScene(entt::entity player)
{
    if (entt::null == player)
    {
		LOG_ERROR << "player is null ";
		return;
    }
    auto p_scene = registry.try_get<SceneEntity>(player);
    auto player_id = registry.get<Guid>(player);
    if (nullptr == p_scene)
    {
        LOG_ERROR << "player do not enter scene " << player_id;
        return;
    }
    Ms2GsEnterSceneRequest enter_scene_message;

    auto p_scene_info = registry.try_get<SceneInfo>((*p_scene).scene_entity_);
    if (nullptr == p_scene_info)
    {
        LOG_ERROR << "scene info " << player_id;
        return;
    }
    enter_scene_message.set_scene_id(p_scene_info->scene_id());
    auto try_player_session = registry.try_get<PlayerSession>(player);
    if (nullptr == try_player_session)
    {
        LOG_ERROR << "player session not valid" << player_id;
        return;
    }
    enter_scene_message.set_session_id(try_player_session->session_id());
    Send2GsPlayer(enter_scene_message, player);
}


void PlayerSceneSystem::SendEnterGs(entt::entity player)
{
	auto try_player_session = registry.try_get<PlayerSession>(player);
	if (nullptr == try_player_session)
	{
		LOG_ERROR << "send enter gs player session not found" << registry.get<Guid>(player);
		return;
	}
	auto it = g_gs_nodes.find(try_player_session->gs_node_id());
	if (it != g_gs_nodes.end())
	{
		gsservice::EnterGsRequest message;
		message.set_player_id(registry.get<Guid>(player));
		message.set_session_id(try_player_session->session_id());
		message.set_ms_node_id(master_node_id());
		registry.get<GsStubPtr>(it->second)->CallMethod(message, &gsservice::GsService_Stub::EnterGs);
	}
}

uint32_t PlayerSceneSystem::ChangeScene(entt::entity player, entt::entity to_scene)
{
    if (nullptr != registry.try_get<AfterChangeGsEnterScene>(player))
    {
        LOG_ERROR << " changing scene " << registry.get<Guid>(player);
        return kRetEnterSceneChangingGs;
    }
	auto try_to_scene_gs = registry.try_get<GsNodePtr>(to_scene);
	auto p_player_gs = registry.try_get<PlayerSession>(player);
	if (nullptr == try_to_scene_gs || nullptr == p_player_gs)
	{
		LOG_ERROR << " scene null : " << (nullptr == try_to_scene_gs) << " " << (nullptr == p_player_gs);
		return kRetEnterSceneChangeSceneOffLine;
	}
    LeaveSceneParam lp;
    lp.leaver_ = player;
    ScenesSystem::LeaveScene(lp);
    
	auto& p_to_scene_gs = *try_to_scene_gs;
	//同gs之间的切换
	Ms2GsLeaveSceneRequest leave_scene_message;
	if (p_player_gs->gs_node_id() == p_to_scene_gs->node_id())
	{
        leave_scene_message.set_change_gs(false);
		EnterSceneParam ep;
		ep.enterer_ = player;
		ep.scene_ = to_scene;
		ScenesSystem::EnterScene(ep);
		
	}
	else
	{
        leave_scene_message.set_change_gs(true);

		//正在切换
		//切换gs  存储完毕之后才能进入下一个场景
		//放到存储完毕切换场景的队列里面，如果等够足够时间没有存储完毕，可能就是服务器崩溃了,注意，是可能 
		auto& change_gs_scene = registry.emplace<AfterChangeGsEnterScene>(player);
		change_gs_scene.set_gs_node_id(p_to_scene_gs->node_id());
		change_gs_scene.mutable_scene_info()->set_scene_id(registry.get<SceneInfo>(to_scene).scene_id());
	}
	Send2GsPlayer(leave_scene_message, player);
	return kRetOK;
}
