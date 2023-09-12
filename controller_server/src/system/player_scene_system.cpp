#include "player_scene_system.h"

#include "muduo/base/Logging.h"

#include "src/comp/player_list.h"
#include "src/controller_server.h"
#include "src/game_logic/comp/scene_comp.h"
#include "src/game_logic/scene/scene_system.h"

#include "src/game_logic/tips_id.h"
#include "src/network/message_system.h"
#include "src/network/player_session.h"
#include "src/system/player_tip_system.h"
#include "src/system/player_change_scene.h"
#include "src/pb/pbc/lobby_scene_service.h"
#include "src/pb/pbc/game_service_service.h"
#include "src/pb/pbc/game_scene_server_player_service.h"
#include "src/thread_local/controller_thread_local_storage.h"
#include "src/pb/pbc/component_proto/player_network_comp.pb.h"

NodeId controller_node_id();

void PlayerSceneSystem::Send2GsEnterScene(entt::entity player)
{
    if (entt::null == player)
    {
		LOG_ERROR << "player is null ";
		return;
    }
    const auto p_scene = tls.registry.try_get<SceneEntity>(player);
    const auto player_id = tls.registry.get<Guid>(player);
    if (nullptr == p_scene)
    {
        LOG_ERROR << "player do not enter scene " << player_id;
        return;
    }

    const auto scene_info = tls.registry.try_get<SceneInfo>((*p_scene).scene_entity_);
    if (nullptr == scene_info)
    {
        LOG_ERROR << "scene info " << player_id;
        return;
    }

    const auto player_node_info = tls.registry.try_get<PlayerNodeInfo>(player);
    if (nullptr == player_node_info)
    {
        LOG_ERROR << "player session not valid" << player_id;
        return;
    }
    Ctlr2GsEnterSceneRequest enter_scene_message;
    enter_scene_message.set_scene_id(scene_info->guid());
    enter_scene_message.set_player_id(player_id);
    CallGameNodeMethod(GameServiceEnterSceneMsgId, enter_scene_message, player_node_info->game_node_id());
}


void PlayerSceneSystem::EnterSceneS2C(entt::entity player)
{
    EnterSceneS2CRequest msg;
    CallGamePlayerMethod(GamePlayerSceneServiceEnterSceneS2CMsgId, msg, player);
}

NodeId PlayerSceneSystem::GetGsNodeIdByScene(const entt::entity scene)
{
    const auto* p_gs_data = tls.registry.try_get<GsNodePtr>(scene);
    //找不到gs了，放到好的gs里面
    if (nullptr == p_gs_data)
    {
        return kInvalidU32Id;
    }
    return (*p_gs_data)->node_id();
}


void PlayerSceneSystem::CallPlayerEnterGs(entt::entity player, NodeId node_id, SessionId session_id)
{
    //todo gs崩溃
    GameNodeEnterGsRequest req;
    req.set_player_id(tls.registry.get<Guid>(player));
    req.set_session_id(session_id);
    req.set_controller_node_id(controller_node_id());
    CallGameNodeMethod(GameServiceEnterGsMsgId, req, node_id);
}

//前一个队列完成的时候才应该调用到这里去判断当前队列
void PlayerSceneSystem::TryEnterNextScene(entt::entity player)
{
    auto* const try_change_scene_queue = tls.registry.try_get<PlayerControllerChangeSceneQueue>(player);
    if (nullptr == try_change_scene_queue)
    {
        return;
    }
    auto& change_scene_queue = try_change_scene_queue->change_scene_queue_;
    auto* const try_from_scene = tls.registry.try_get<SceneEntity>(player);
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
    auto to_scene_guid = change_scene_info.scene_info().guid();
    entt::entity to_scene = entt::null;
    //用scene_config id 去换本服的controller
    if (to_scene_guid <= 0)
    {
        GetSceneParam getp;
        getp.scene_conf_id_ = change_scene_info.scene_info().scene_confid();
        to_scene = ServerNodeSystem::GetSceneOnMinPlayerSizeNode(getp);
        if (entt::null == to_scene)
        {
            PlayerTipSystem::Tip(player, kRetEnterSceneSceneFull, {});
            PlayerChangeSceneSystem::PopFrontChangeSceneQueue(player);
            return;
        }
        to_scene_guid = tls.registry.get<SceneInfo>(to_scene).guid();
    }
    else
    {
        to_scene = ScenesSystem::GetSceneByGuid(to_scene_guid);
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

    auto from_scene_id = tls.registry.get<SceneInfo>(try_from_scene->scene_entity_).guid();
    if (to_scene_guid == from_scene_id)
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
            rq.set_scene_id(to_scene_guid);
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