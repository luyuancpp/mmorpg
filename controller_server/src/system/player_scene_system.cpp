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

#include "component_proto/player_login_comp.pb.h"
#include "logic_proto/lobby_scene.pb.h"
#include "logic_proto/scene_server_player.pb.h"
#include "game_service.pb.h"

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
    Send2GsPlayer(msg, player);
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
    gsservice::EnterGsRequest message;
    message.set_player_id(registry.get<Guid>(player));
    message.set_session_id(session_id);
    message.set_controller_node_id(controller_node_id());
    registry.get<GsStubPtr>(it->second)->CallMethod(message, &gsservice::GsService_Stub::EnterGs);
}


using EnterLobbyMainSceneRpc = std::shared_ptr<NormalClosure<lobbyservcie::EnterCrossMainSceneRequest, lobbyservcie::EnterCrossMainSceneResponese>>;
void EnterLobbyMainSceneReplied(EnterLobbyMainSceneRpc replied)
{
    // todo ����л����У�return error
    //�п絽b�������У����û�������е�c����������ٵ�cĿǰ�Ͳ�������������ˣ����ǵĻ�д�����鷳
    //todo �첽���������֮ǰ��ȥ�л������������Ѿ��л�����ĳ����ˣ����еĻ����ܾͲ����ˣ���������������ˣ������˲�������ô��
    auto player = g_player_list->GetPlayer(replied->s_rq_.player_id());
    if (entt::null == player)
    {
        LOG_ERROR << "player not found" << replied->s_rq_.player_id();
        return;
    }
    GetPlayerCompnentMemberReturnVoid(change_scene_queue, PlayerControllerChangeSceneQueue);
    if (change_scene_queue.empty())
    {
        return;
    }
    auto scene = ScenesSystem::get_scene(replied->s_rq_.scene_id());
    if (entt::null == scene)
    {
        LOG_ERROR << "scene not found" << replied->s_rq_.scene_id();
        PlayerChangeSceneSystem::PopFrontChangeSceneQueue(player);
        return;
    }
    auto& change_scene_info = change_scene_queue.front();
    change_scene_info.set_change_cross_server_status(ControllerChangeSceneInfo::eEnterCrossServerSceneSucceed);
    PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);
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
    auto scene_id = scene_info.scene_id();
    entt::entity to_scene = entt::null;
    if (scene_id <= 0)//��scene_config id ȥ��������controller
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
        scene_id = registry.get<SceneInfo>(to_scene).scene_id();
    }
    else
    {
        to_scene = ScenesSystem::get_scene(scene_id);
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
        LOG_ERROR << " scene null : " << (nullptr == try_from_scene_gs) << " " << (nullptr == try_to_scene_gs);
        PlayerChangeSceneSystem::PopFrontChangeSceneQueue(player);
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
            lobbyservcie::LeaveCrossMainSceneRequest rpc;
            rpc.set_player_id(registry.get<Guid>(player));
            g_controller_node->lobby_stub().CallMethod(rpc, &lobbyservcie::LobbyService_Stub::LeaveCrossMainScene);
        }
        if (is_to_gs_is_cross_server)
        {
            //ע����Ȼһ���߼������ǲ�һ������leave���洦��
            EnterLobbyMainSceneRpc rpc(std::make_shared<EnterLobbyMainSceneRpc::element_type>());
            rpc->s_rq_.set_scene_id(registry.get<SceneInfo>(to_scene).scene_id());
            rpc->s_rq_.set_player_id(registry.get<Guid>(player));
            g_controller_node->lobby_stub().CallMethod(EnterLobbyMainSceneReplied, rpc, &lobbyservcie::LobbyService_Stub::EnterCrossMainScene);
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