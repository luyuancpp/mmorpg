#include "player_scene.h"

#include "muduo/base/Logging.h"

#include "centre_node.h"
#include "comp/scene.h"
#include "system/scene/scene_system.h"

#include "constants/tips_id.h"
#include "network//game_node.h"
#include "network/message_system.h"
#include "service/game_scene_server_player_service.h"
#include "service/game_service_service.h"
#include "system/player_change_scene.h"
#include "system/player_tip.h"

#include "component_proto/player_network_comp.pb.h"
#include "component_proto/player_scene_comp.pb.h"

void PlayerSceneSystem::OnLoginEnterScene(entt::entity player)
{
    if (!tls.registry.valid(player))
    {
        LOG_ERROR << "player not found";
        return;
    }
    const auto scene_info_comp = tls.registry.try_get<PlayerSceneInfoComp>(player);
    if (nullptr == scene_info_comp)
    {
        LOG_ERROR << "player not found";
        return;
    }
    entt::entity scene_id = entt::entity{ scene_info_comp->scene_info().guid() };
    entt::entity scene_id_last_time =
        entt::entity{ scene_info_comp->scene_info_last_time().guid() };

    bool can_enter_scene = false;
    bool can_enter_scene_last_time = false;

    //之前的场景有效
    if (tls.scene_registry.valid(scene_id))
    {
        //但是进不去
        if (kRetOK == ScenesSystem::CheckEnterScene(
            { .scene_ = scene_id, 
            .player_ = player }))
        {
            can_enter_scene = true;
        }
    }
    else if (tls.scene_registry.valid(scene_id_last_time))
    {
        if (kRetOK == ScenesSystem::CheckEnterScene(
            { .scene_ = scene_id_last_time, 
            .player_ = player }))
        {
            can_enter_scene_last_time = true;
        }
    }

    entt::entity scene = entt::null;
    if (can_enter_scene)
    {
        scene = scene_id;
    }
    else if (can_enter_scene_last_time)
    {
        scene = scene_id_last_time;
    }
    else
    {
        if (scene_info_comp->scene_info().scene_confid() > 0)
        {
            scene = NodeSceneSystem::GetNotFullScene({ scene_info_comp->scene_info().scene_confid() });
            if (entt::null == scene)
            {
                scene = NodeSceneSystem::GetNotFullScene({ scene_info_comp->scene_info().scene_confid() });
            }
        }
    }
    
    //找不到上次的场景,或者上次场景满了，放到默认场景里面
    if (scene == entt::null)
    {
        scene = NodeSceneSystem::GetNotFullScene({ GetDefaultSceneConfigId() });
    }

    if (scene == entt::null)
    {
        LOG_ERROR << "player login enter scene ";
        return;
    }
    //todo 会话没有了玩家还在
    CallPlayerEnterGs(player, ScenesSystem::get_game_node_id(scene));
    CentreChangeSceneInfo change_scene_info;
    PlayerChangeSceneSystem::CopyTo(change_scene_info, tls.scene_registry.get<SceneInfo>(scene));
    change_scene_info.set_change_gs_type(CentreChangeSceneInfo::eDifferentGs);
    change_scene_info.set_change_gs_status(CentreChangeSceneInfo::eEnterGsSceneSucceed);
    PlayerChangeSceneSystem::PushChangeSceneInfo(player, change_scene_info);
}

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

    const auto scene_info = tls.scene_registry.try_get<SceneInfo>((*p_scene).scene_entity_);
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
    Centre2GsEnterSceneRequest request;
    request.set_scene_id(scene_info->guid());
    request.set_player_id(player_id);
    CallGameNodeMethod(GameServiceEnterSceneMsgId, request, player_node_info->game_node_id());

    LOG_DEBUG << "player enter scene " << player_id << " "
        << scene_info->guid()  << " " 
        << player_node_info->game_node_id();
}


void PlayerSceneSystem::CallPlayerEnterGs(entt::entity player, NodeId node_id)
{
    const auto player_node_info = tls.registry.try_get<PlayerNodeInfo>(player);
    if (nullptr == player_node_info)
    {
        return;
    }
    //todo gs崩溃
    GameNodeEnterGsRequest rq;
    rq.set_player_id(tls.registry.get<Guid>(player));
    rq.set_session_id((*player_node_info).gate_session_id());
    rq.set_centre_node_id(g_centre_node->GetNodeId());
    CallGameNodeMethod(GameServiceEnterGsMsgId, rq, node_id);
}

//前一个队列完成的时候才应该调用到这里去判断当前队列
void PlayerSceneSystem::TryEnterNextScene(entt::entity player)
{
    auto* const change_scene_queue = tls.registry.try_get<PlayerCentreChangeSceneQueue>(player);
    if (nullptr == change_scene_queue)
    {
        return;
    }
    const auto* const from_scene = tls.registry.try_get<SceneEntity>(player);
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
    auto to_scene_guid = change_scene_info.guid();
    entt::entity to_scene = entt::null;
    //用scene_config id 去换本服的centre
    if (to_scene_guid <= 0)
    {
        GetSceneParam getp;
        getp.scene_conf_id_ = change_scene_info.scene_confid();
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
        to_scene = entt::entity{to_scene_guid};
        if (entt::null == to_scene)
        {
            PlayerTipSystem::Tip(player, kRetEnterSceneSceneNotFound, {});
            PlayerChangeSceneSystem::PopFrontChangeSceneQueue(player);
            return;
        }
    }
    const auto from_scene_info = tls.scene_registry.try_get<SceneInfo>(from_scene->scene_entity_);
    if (nullptr == from_scene_info)
    {
        return;
    }
    const auto from_scene_game_node = ScenesSystem::get_game_node_eid(from_scene_info->guid()) ;
    const auto to_scene_game_node   = ScenesSystem::get_game_node_eid(to_scene_guid);
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

    //不是跨服才在本地判断,跨服有自己的判断
    if (!change_scene_info.ignore_full())
    {
        if ( const auto ret = ScenesSystem::CheckScenePlayerSize(to_scene) ; kRetOK != ret)
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
            change_scene_info.set_change_gs_type(CentreChangeSceneInfo::eSameGs);
        }
        else if (from_scene_game_node != to_scene_game_node)
        {
            change_scene_info.set_change_gs_type(CentreChangeSceneInfo::eDifferentGs);
        }
    }

    //跨服间切换,如果另一个跨服满了就不应该进去了
    //如果是跨服，就应该先跨服去处理
    //原来服务器之间换场景，不用通知跨服离开场景
    //todo 如果是进出镜像，一定保持在原来的gs切换,主世界分线和镜像没关系，这样就节省了玩家切换流程，效率也提高了
    //todo 跨服的时候重新上线
     //跨服到原来服务器，通知跨服离开场景，todo注意回到原来服务器的时候可能原来服务器满了

    PlayerChangeSceneSystem::TryProcessChangeSceneQueue(player);//不跨服就开始处理同一个gs 或者不同gs
}

uint32_t PlayerSceneSystem::GetDefaultSceneConfigId()
{
    return 1;
}
