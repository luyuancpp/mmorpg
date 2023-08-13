#include "player_scene_system.h"

#include "muduo/base/Logging.h"

#include "src/comp/player_list.h"
#include "src/controller_server.h"
#include "src/game_logic/comp/scene_comp.h"
#include "src/game_logic/scene/scene.h"

#include "src/game_logic/tips_id.h"
#include "src/network/message_system.h"
#include "src/network/player_session.h"
#include "src/system/player_tip_system.h"
#include "src/system/player_change_scene.h"
#include "src/pb/pbc/lobby_scene_service.h"
#include "src/pb/pbc/game_service_service.h"
#include "src/pb/pbc/game_scene_server_player_service.h"
#include "src/thread_local/controller_thread_local_storage.h"


void PlayerSceneSystem::Send2GsEnterScene(entt::entity player)
{
    if (entt::null == player)
    {
		LOG_ERROR << "player is null ";
		return;
    }
    auto p_scene = tls.registry.try_get<SceneEntity>(player);
    auto player_id = tls.registry.get<Guid>(player);
    if (nullptr == p_scene)
    {
        LOG_ERROR << "player do not enter scene " << player_id;
        return;
    }
 
    auto p_scene_info = tls.registry.try_get<SceneInfo>((*p_scene).scene_entity_);
    if (nullptr == p_scene_info)
    {
        LOG_ERROR << "scene info " << player_id;
        return;
    }

    auto try_player_session = tls.registry.try_get<PlayerSession>(player);
    if (nullptr == try_player_session)
    {
        LOG_ERROR << "player session not valid" << player_id;
        return;
    }
    Ctlr2GsEnterSceneRequest enter_scene_message;
    enter_scene_message.set_scene_id(p_scene_info->scene_id());
    enter_scene_message.set_player_id(player_id);

    auto gs = try_player_session->gs();
    if (nullptr == gs)
    {
        LOG_INFO << "gs not found ";
        return;
    }
    gs->session_.CallMethod(GameServiceEnterSceneMsgId, enter_scene_message);
}


void PlayerSceneSystem::EnterSceneS2C(entt::entity player)
{
    EnterSceneS2CRequest msg;
    CallGsPlayerMethod(GamePlayerSceneServiceEnterSceneS2CMsgId, msg, player);
}

NodeId PlayerSceneSystem::GetGsNodeIdByScene(entt::entity scene)
{
    auto* p_gs_data = tls.registry.try_get<GsNodePtr>(scene);
    if (nullptr == p_gs_data)//找不到gs了，放到好的gs里面
    {
        return kInvalidU32Id;
    }
    return (*p_gs_data)->node_id();
}


void PlayerSceneSystem::CallPlayerEnterGs(entt::entity player, NodeId node_id, SessionId session_id)
{
    //todo gs崩溃
	auto it = controller_tls.game_node().find(node_id);
	if (it == controller_tls.game_node().end())
	{
        return;
    }
    GameNodeEnterGsRequest rq;
    rq.set_player_id(tls.registry.get<Guid>(player));
    rq.set_session_id(session_id);
    rq.set_controller_node_id(controller_node_id());
    tls.registry.get<GsNodePtr>(it->second)->session_.CallMethod(GameServiceEnterGsMsgId, rq);
}

//todo clear code 
//前一个队列完成的时候才应该调用到这里去判断当前队列
void PlayerSceneSystem::TryEnterNextScene(entt::entity player)
{
    GetPlayerComponentMemberReturnVoid(change_scene_queue, PlayerControllerChangeSceneQueue);
    if (change_scene_queue.empty())
    {
        return;
    }
    auto try_from_scene = tls.registry.try_get<SceneEntity>(player);
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
    auto to_scene_id = change_scene_info.scene_info().scene_id();
    entt::entity to_scene = entt::null;
    if (to_scene_id <= 0)//用scene_config id 去换本服的controller
    {
        GetSceneParam getp;
        getp.scene_confid_ = change_scene_info.scene_info().scene_confid();
        to_scene = ServerNodeSystem::GetWeightRoundRobinMainScene(getp);
        if (entt::null == to_scene)
        {
            PlayerTipSystem::Tip(player, kRetEnterSceneSceneFull, {});
            PlayerChangeSceneSystem::PopFrontChangeSceneQueue(player);
            return;
        }
        to_scene_id = tls.registry.get<SceneInfo>(to_scene).scene_id();
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
    auto try_from_scene_gs = tls.registry.try_get<GsNodePtr>(try_from_scene->scene_entity_);
    auto try_to_scene_gs = tls.registry.try_get<GsNodePtr>(to_scene);
    if (nullptr == try_from_scene_gs || nullptr == try_to_scene_gs)
    {
        LOG_ERROR << " gs component null : " << (nullptr == try_from_scene_gs) << " " << (nullptr == try_to_scene_gs);
        PlayerChangeSceneSystem::PopFrontChangeSceneQueue(player);
        return;
    }

    auto from_scene_id = tls.registry.get<SceneInfo>(try_from_scene->scene_entity_).scene_id();
    if (to_scene_id == from_scene_id)
    {
        PlayerTipSystem::Tip(player, kRetEnterSceneYouInCurrentScene, {});
        PlayerChangeSceneSystem::PopFrontChangeSceneQueue(player);
        return;
    }

    auto from_gs_it = controller_tls.game_node().find((*try_from_scene_gs)->node_id());
    auto to_gs_it = controller_tls.game_node().find((*try_to_scene_gs)->node_id());
    if (from_gs_it == controller_tls.game_node().end() || to_gs_it == controller_tls.game_node().end())
    {
        //服务器已经崩溃了
        LOG_ERROR << " gs not found  : " <<
            (*try_from_scene_gs)->node_id() <<
            " " << (*try_to_scene_gs)->node_id();
        PlayerChangeSceneSystem::PopFrontChangeSceneQueue(player);
        return;
    }
    entt::entity from_gs = from_gs_it->second;
    entt::entity to_gs = to_gs_it->second;
    bool is_from_gs_is_cross_server = tls.registry.any_of<CrossMainSceneServer>(from_gs);
    bool is_to_gs_is_cross_server = tls.registry.any_of<CrossMainSceneServer>(to_gs);

    //不是跨服才在本地判断,跨服有自己的判断
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

    //跨服间切换,如果另一个跨服满了就不应该进去了
    //如果是跨服，就应该先跨服去处理
    //原来服务器之间换场景，不用通知跨服离开场景
    //todo 如果是进出镜像，一定保持在原来的gs切换,主世界分线和镜像没关系，这样就节省了玩家切换流程，效率也提高了
    //todo 跨服的时候重新上线
    //目标场景是跨服场景，通知跨服去换,跨服只做人数检测，不做其他的事情

    if (is_from_gs_is_cross_server || is_to_gs_is_cross_server)
    {
        change_scene_info.set_change_cross_server_type(ControllerChangeSceneInfo::eCrossServer);
        change_scene_info.set_change_cross_server_status(ControllerChangeSceneInfo::eEnterCrossServerScene);
        if (is_from_gs_is_cross_server)
        {
            //跨服到原来服务器，通知跨服离开场景，todo注意回到原来服务器的时候可能原来服务器满了
            LeaveCrossMainSceneRequest rq;
            rq.set_player_id(tls.registry.get<Guid>(player));
            g_controller_node->lobby_node()->CallMethod(LobbyServiceLeaveCrossMainSceneMsgId, rq);
        }
        if (is_to_gs_is_cross_server)
        {
            //注意虽然一个逻辑，但是不一定是在leave后面处理
            EnterCrossMainSceneRequest rq;
            rq.set_scene_id(to_scene_id);
            rq.set_player_id(tls.registry.get<Guid>(player));
            g_controller_node->lobby_node()->CallMethod(LobbyServiceEnterCrossMainSceneMsgId, rq);
            return;
        }
    }
    else
    {
        change_scene_info.set_change_cross_server_type(ControllerChangeSceneInfo::eDotnotCrossServer);
    }

    if (ControllerChangeSceneInfo::eDotnotCrossServer == change_scene_info.change_cross_server_status())
    {
        PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);//不跨服就开始处理同一个gs 或者不同gs
        return;
    }

}