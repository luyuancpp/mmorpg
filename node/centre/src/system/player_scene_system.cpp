#include "player_scene_system.h"

#include "muduo/base/Logging.h"

#include "src/system/centre_player_system.h"
#include "src/centre_node.h"
#include "src/comp/scene_comp.h"
#include "src/system/scene/scene_system.h"

#include "src/constants/tips_id.h"
#include "src/network/message_system.h"
#include "src/system/player_tip_system.h"
#include "src/system/player_change_scene.h"
#include "service/game_service_service.h"
#include "service/game_scene_server_player_service.h"
#include "src/thread_local/centre_thread_local_storage.h"
#include "src/network//game_node.h"

#include "component_proto/player_network_comp.pb.h"

NodeId centre_node_id();

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

void PlayerSceneSystem::CallPlayerEnterGs(entt::entity player, NodeId node_id, SessionId session_id)
{
    //todo gs崩溃
    GameNodeEnterGsRequest req;
    req.set_player_id(tls.registry.get<Guid>(player));
    req.set_session_id(session_id);
    req.set_centre_node_id(centre_node_id());
    CallGameNodeMethod(GameServiceEnterGsMsgId, req, node_id);
}

//前一个队列完成的时候才应该调用到这里去判断当前队列
void PlayerSceneSystem::TryEnterNextScene(entt::entity player)
{
    auto* const change_scene_queue = tls.registry.try_get<PlayerCentreChangeSceneQueue>(player);
    if (nullptr == change_scene_queue)
    {
        return;
    }
    auto* const from_scene = tls.registry.try_get<SceneEntity>(player);
    if (nullptr == from_scene)
    {
        PlayerTipSystem::Tip(player, kRetEnterSceneYourSceneIsNull, {});// todo 
        return;
    }
    auto& change_scene_info = change_scene_queue->change_scene_queue_.front();
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
        to_scene = NodeSceneSystem::GetNotFullScene(getp);
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
        to_scene = entt::to_entity(to_scene_guid);
        if (entt::null == to_scene)
        {
            PlayerTipSystem::Tip(player, kRetEnterSceneSceneNotFound, {});
            PlayerChangeSceneSystem::PopFrontChangeSceneQueue(player);
            return;
        }
    }
    auto from_scene_info = tls.game_node_registry.try_get<SceneInfo>(from_scene->scene_entity_);
    if (nullptr == from_scene_info)
    {
        return;
    }
    auto from_scene_game_node = ScenesSystem::get_game_node_eid(from_scene_info->guid()) ;
    auto to_scene_game_node = ScenesSystem::get_game_node_eid(to_scene_guid);
    if (!tls.game_node_registry.valid(from_scene_game_node) || 
        !tls.game_node_registry.valid(to_scene_game_node))
    {
        LOG_ERROR << "scene not found " << from_scene_info->guid() << " " << to_scene_guid;
        PlayerChangeSceneSystem::PopFrontChangeSceneQueue(player);
        return;
    }

    if (to_scene_guid == from_scene_info->guid())
    {
        PlayerTipSystem::Tip(player, kRetEnterSceneYouInCurrentScene, {});
        PlayerChangeSceneSystem::PopFrontChangeSceneQueue(player);
        return;
    }

    bool is_from_gs_is_cross_server = tls.game_node_registry.any_of<CrossMainSceneServer>(from_scene_game_node);
    bool is_to_gs_is_cross_server = tls.game_node_registry.any_of<CrossMainSceneServer>(to_scene_game_node);

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

    if (entt::null != from_scene_game_node)
    {
        if (from_scene_game_node == to_scene_game_node)
        {
            change_scene_info.set_change_gs_type(ControllerChangeSceneInfo::eSameGs);
        }
        else if (from_scene_game_node != to_scene_game_node)
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
            //LeaveCrossMainSceneRequest rq;
            //rq.set_player_id(tls.registry.get<Guid>(player));
            //g_centre_node->lobby_node()->CallMethod(LobbyServiceLeaveCrossMainSceneMsgId, rq);
        }
        if (is_to_gs_is_cross_server)
        {
            //注意虽然一个逻辑，但是不一定是在leave后面处理
           // EnterCrossMainSceneRequest rq;
            //rq.set_scene_id(to_scene_guid);
            //rq.set_player_id(tls.registry.get<Guid>(player));
            //g_centre_node->lobby_node()->CallMethod(LobbyServiceEnterCrossMainSceneMsgId, rq);
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