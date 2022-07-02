#include "player_scene_system.h"

#include "muduo/base/Logging.h"

#include "src/game_logic/comp/scene_comp.h"
#include "src/game_logic/scene/scene.h"
#include "src/game_logic/scene/servernode_system.h"
#include "src/network/message_system.h"
#include "src/network/player_session.h"


#include "component_proto/player_login_comp.pb.h"
#include "logic_proto/scene_server_player.pb.h"
#include "gs_service.pb.h"

using GsStubPtr = std::unique_ptr<RpcStub<gsservice::GsService_Stub>>;

void PlayerSceneSystem::OnEnterScene(entt::entity player)
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
    Ms2GsEnterSceneRequest message;
	auto try_enter_gs = registry.try_get<EnterGsFlag>(player);
	if (nullptr != try_enter_gs)
	{
		message.set_enter_gs_type((*try_enter_gs).enter_gs_type());
		registry.remove<EnterGsFlag>(player);
	}
    auto p_scene_info = registry.try_get<SceneInfo>((*p_scene).scene_entity());
    if (nullptr == p_scene_info)
    {
        LOG_ERROR << "scene info " << player_id;
        return;
    }
    message.set_scene_id(p_scene_info->scene_id());
    auto try_player_session = registry.try_get<PlayerSession>(player);
    if (nullptr == try_player_session)
    {
        LOG_ERROR << "player session not valid" << player_id;
        return;
    }
    message.set_session_id(try_player_session->session_id());
    Send2GsPlayer(message, player);
}

void PlayerSceneSystem::OnLeaveScene(entt::entity player, bool change_gs)
{
    Ms2GsLeaveSceneRequest message;
    message.set_change_gs(change_gs);
    Send2GsPlayer(message, player);
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

void PlayerSceneSystem::ChangeScene(entt::entity player, entt::entity scene)
{
	auto try_scene_gs = registry.try_get<GsNodePtr>(scene);
	auto p_player_gs = registry.try_get<PlayerSession>(player);
	if (nullptr == try_scene_gs || nullptr == p_player_gs)
	{
		LOG_ERROR << " scene null : " << (nullptr == try_scene_gs) << " " << (nullptr == p_player_gs);
		return;
	}

	auto& p_scene_gs = *try_scene_gs;
	//ͬgs֮����л�
	if (p_player_gs->gs_node_id() == p_scene_gs->node_id())
	{
		LeaveSceneParam lp;
		lp.leaver_ = player;
		EnterSceneParam ep;
		ep.enterer_ = player;
		ep.scene_ = scene;
		//todo ��δ��벻��������֪��Ҫ��ɶ
		//����ǿ������֪ͨ����뿪����
		ScenesSystem::GetSingleton().LeaveScene(lp);
		PlayerSceneSystem::OnLeaveScene(player, false);
		ScenesSystem::GetSingleton().EnterScene(ep);
		PlayerSceneSystem::OnEnterScene(player);
	}
	else
	{
		LeaveSceneParam lp;
		lp.leaver_ = player;
		//�л�gs  �洢���֮����ܽ�����һ������
		ScenesSystem::GetSingleton().LeaveScene(lp);
		PlayerSceneSystem::OnLeaveScene(player, true);

		//�ŵ��洢����л������Ķ������棬����ȹ��㹻ʱ��û�д洢��ϣ����ܾ��Ƿ�����������,ע�⣬�ǿ��� 
		auto& change_gs_scene = registry.emplace_or_replace<AfterChangeGsEnterScene>(player);
		change_gs_scene.mutable_scene_info()->set_scene_id(registry.get<SceneInfo>(scene).scene_id());
	}
}
