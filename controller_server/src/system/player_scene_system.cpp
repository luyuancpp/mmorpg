#include "player_scene_system.h"

#include "muduo/base/Logging.h"

#include "src/comp/player_list.h"
#include "src/controller_server.h"
#include "src/game_logic/comp/scene_comp.h"
#include "src/game_logic/scene/scene.h"
#include "src/game_logic/scene/servernode_system.h"
#include "src/game_logic/tips_id.h"
#include "src/network/message_system.h"
#include "src/network/player_session.h"
#include "src/system/player_tip_system.h"
#include "src/system/player_change_scene.h"
#include "src/pb/pbc/service_method/lobby_scenemethod.h"
#include "src/pb/pbc/service_method/game_servicemethod.h"

#include "component_proto/player_login_comp.pb.h"
#include "logic_proto/scene_server_player.pb.h"
#include "game_service.pb.h"

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
    Controller2GsEnterSceneRequest enter_scene_message;

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


void PlayerSceneSystem::EnterSceneS2C(entt::entity player)
{
    EnterSceneS2CRequest msg;
    CallGsPlayerMethod(msg, player);
}

NodeId PlayerSceneSystem::GetGsNodeIdByScene(entt::entity scene)
{
    auto* p_gs_data = registry.try_get<GsNodePtr>(scene);
    if (nullptr == p_gs_data)//�Ҳ���gs�ˣ��ŵ��õ�gs����
    {
        return kInvalidU32Id;
    }
    return (*p_gs_data)->node_id();
}


void PlayerSceneSystem::CallPlayerEnterGs(entt::entity player, NodeId node_id, SessionId session_id)
{
    //todo gs����
	auto it = g_gs_nodes.find(node_id);
	if (it == g_gs_nodes.end())
	{
        return;
    }
    gsservice::EnterGsRequest rq;
    rq.set_player_id(registry.get<Guid>(player));
    rq.set_session_id(session_id);
    rq.set_controller_node_id(controller_node_id());
    registry.get<GsNodePtr>(it->second)->session_.CallMethod(gsserviceEnterGsMethoddesc, &rq);
}


//ǰһ��������ɵ�ʱ���Ӧ�õ��õ�����ȥ�жϵ�ǰ����
void PlayerSceneSystem::TryEnterNextScene(entt::entity player)
{
    GetPlayerCompnentMemberReturnVoid(change_scene_queue, PlayerControllerChangeSceneQueue);
    if (change_scene_queue.empty())
    {
        return;
    }
    auto try_from_scene = registry.try_get<SceneEntity>(player);
    if (nullptr == try_from_scene)
    {
        PlayerTipSystem::Tip(player, kRetEnterSceneYourSceneIsNull, {});// todo 
        return;
    }
    auto& change_scene_info = change_scene_queue.front();
    if (change_scene_info.processing())
    {
        return;
    }
    change_scene_info.set_processing(true);
    auto& scene_info = change_scene_info.scene_info();
    auto to_scene_id = scene_info.scene_id();
    entt::entity to_scene = entt::null;
    if (to_scene_id <= 0)//��scene_config id ȥ��������controller
    {
        GetSceneParam getp;
        getp.scene_confid_ = scene_info.scene_confid();
        to_scene = ServerNodeSystem::GetWeightRoundRobinMainScene(getp);
        if (entt::null == to_scene)
        {
            PlayerTipSystem::Tip(player, kRetEnterSceneSceneFull, {});
            PlayerChangeSceneSystem::PopFrontChangeSceneQueue(player);
            return;
        }
        to_scene_id = registry.get<SceneInfo>(to_scene).scene_id();
    }
    else
    {
        to_scene = ScenesSystem::get_scene(to_scene_id);
        if (entt::null == to_scene)
        {
            PlayerTipSystem::Tip(player, kRetEnterSceneSceneNotFound, {});
            PlayerChangeSceneSystem::PopFrontChangeSceneQueue(player);
            return;
        }
    }

    auto try_from_scene_gs = registry.try_get<GsNodePtr>(try_from_scene->scene_entity_);
    auto try_to_scene_gs = registry.try_get<GsNodePtr>(to_scene);
    if (nullptr == try_from_scene_gs || nullptr == try_to_scene_gs)
    {
        LOG_ERROR << " gs compnent null : " << (nullptr == try_from_scene_gs) << " " << (nullptr == try_to_scene_gs);
        PlayerChangeSceneSystem::PopFrontChangeSceneQueue(player);
        return;
    }

    auto from_scene_id = registry.get<SceneInfo>(try_from_scene->scene_entity_).scene_id();
    if (to_scene_id == from_scene_id)
    {
        PlayerTipSystem::Tip(player, kRetEnterSceneYouInCurrentScene, {});
        return;
    }

    auto from_gs_it = g_gs_nodes.find((*try_from_scene_gs)->node_id());
    auto to_gs_it = g_gs_nodes.find((*try_to_scene_gs)->node_id());
    if (from_gs_it == g_gs_nodes.end() || to_gs_it == g_gs_nodes.end())
    {
        LOG_ERROR << " gs not found  : " <<
            (*try_from_scene_gs)->node_id() <<
            " " << (*try_to_scene_gs)->node_id();
        PlayerChangeSceneSystem::PopFrontChangeSceneQueue(player);
        return;
    }
    entt::entity from_gs = from_gs_it->second;
    entt::entity to_gs = to_gs_it->second;
    bool is_from_gs_is_cross_server = registry.any_of<CrossMainSceneServer>(from_gs);
    bool is_to_gs_is_cross_server = registry.any_of<CrossMainSceneServer>(to_gs);

    //���ǿ�����ڱ����ж�,������Լ����ж�
    if (!change_scene_info.ignore_full() && !is_to_gs_is_cross_server)
    {
        auto ret = ScenesSystem::CheckScenePlayerSize(to_scene);
        if (kRetOK != ret)
        {
            PlayerTipSystem::Tip(player, ret, {});
            PlayerChangeSceneSystem::PopFrontChangeSceneQueue(player);
            return;
        }
    }

    if (entt::null != from_gs)
    {
        if (from_gs == to_gs)
        {
            change_scene_info.set_change_gs_type(ControllerChangeSceneInfo::eSameGs);
        }
        else if (from_gs != to_gs)
        {
            change_scene_info.set_change_gs_type(ControllerChangeSceneInfo::eDifferentGs);
        }
    }

    //������л�,�����һ��������˾Ͳ�Ӧ�ý�ȥ��
    //����ǿ������Ӧ���ȿ��ȥ����
    //ԭ��������֮�任����������֪ͨ����뿪����
    //todo ����ǽ�������һ��������ԭ����gs�л�,��������ߺ;���û��ϵ�������ͽ�ʡ������л����̣�Ч��Ҳ�����
    //todo �����ʱ����������
    //Ŀ�곡���ǿ��������֪ͨ���ȥ��,���ֻ��������⣬��������������

    if (is_from_gs_is_cross_server || is_to_gs_is_cross_server)
    {
        change_scene_info.set_change_cross_server_type(ControllerChangeSceneInfo::eCrossServer);
        change_scene_info.set_change_cross_server_status(ControllerChangeSceneInfo::eEnterCrossServerScene);
        if (is_from_gs_is_cross_server)
        {
            //�����ԭ����������֪ͨ����뿪������todoע��ص�ԭ����������ʱ�����ԭ������������
            lobbyservcie::LeaveCrossMainSceneRequest rq;
            rq.set_player_id(registry.get<Guid>(player));
            g_controller_node->lobby_node()->CallMethod(lobbyservcieLeaveCrossMainSceneMethoddesc, &rq);
        }
        if (is_to_gs_is_cross_server)
        {
            //ע����Ȼһ���߼������ǲ�һ������leave���洦��
            lobbyservcie::EnterCrossMainSceneRequest rq;
            rq.set_scene_id(to_scene_id);
            rq.set_player_id(registry.get<Guid>(player));
            g_controller_node->lobby_node()->CallMethod(lobbyservcieEnterCrossMainSceneMethoddesc, &rq);
            return;
        }
    }
    else
    {
        change_scene_info.set_change_cross_server_type(ControllerChangeSceneInfo::eDotnotCrossServer);
    }

    if (ControllerChangeSceneInfo::eDotnotCrossServer == change_scene_info.change_cross_server_status())
    {
        PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);//������Ϳ�ʼ����ͬһ��gs ���߲�ͬgs
        return;
    }

}