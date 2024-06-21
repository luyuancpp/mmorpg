#include "player_common_system.h"

#include "muduo/base/Logging.h"

#include "comp/scene_comp.h"
#include "thread_local/thread_local_storage.h"
#include "service/scene_client_player_service.h"
#include "service/game_server_player_service.h"
#include "network/message_system.h"
#include "service/game_service_service.h"
#include "system/player_change_scene.h"
#include "thread_local/centre_thread_local_storage.h"
#include "thread_local/thread_local_storage_common_logic.h"
#include "util/defer.h"
#include "system/player_scene_system.h"

#include "component_proto/player_login_comp.pb.h"
#include "component_proto/player_comp.pb.h"
#include "component_proto/player_network_comp.pb.h"
#include "common_proto/mysql_database_table.pb.h"
#include "common_proto/centre_service.pb.h"

void PlayerCommonSystem::OnPlayerAsyncLoaded(Guid player_id, const player_centre_database& message)
{
    using PlayerLoadingInfoList = std::unordered_map<Guid, EnterGameL2Ctr>;
    auto& player_loading_list = tls.global_registry.get<PlayerLoadingInfoList>(global_entity());
    defer(player_loading_list.erase(player_id));
    auto it = player_loading_list.find(player_id);
    if ( it == player_loading_list.end() )
    {
        LOG_ERROR << "loading player  error" << player_id;
        return;
    }
    auto player = tls.registry.create();
    if (const auto [fst, snd] = cl_tls.player_list().emplace(player_id, player); !snd)
    {
        LOG_ERROR << "server emplace error" << player_id;
        return;
    }
    auto ret = cl_tls.player_list().emplace(player_id, player);
    if (!ret.second)
    {
        LOG_ERROR << "login create player error" << player_id;
        return;
    }
    tls.registry.emplace_or_replace<PlayerNodeInfo>(player).set_gate_session_id(
        it->second.session_info().session_id());

    tls.registry.emplace<Player>(player);
    tls.registry.emplace<Guid>(player, player_id);
    tls.registry.emplace<player_centre_database>(player, std::move(message));
    tls.registry.emplace<Player>(player);
    PlayerChangeSceneSystem::InitChangeSceneQueue(player);
    //第一次登录
    tls.registry.emplace<EnterGsFlag>(player).set_enter_gs_type(LOGIN_FIRST);
    PlayerSceneSystem::OnLoginEnterScene(player);
    // on loaded db
}

void PlayerCommonSystem::OnPlayerAsyncSaved(Guid player_id, player_centre_database& message)
{

}

void PlayerCommonSystem::OnGateUpdateGameNodeSucceed(entt::entity player)
{
    const auto* const player_node_info = tls.registry.try_get<PlayerNodeInfo>(player);
    if (nullptr == player_node_info)
    {
        LOG_ERROR << "player session not valid";
        return;
    }
    const auto* const player_id = tls.registry.try_get<Guid>(player);
    if (nullptr == player_id)
    {
        LOG_ERROR << "player  not found ";
        return;
    }
    UpdatePlayerSessionRequest message;
    message.set_session_id(player_node_info->gate_session_id());
    message.set_player_id(*player_id);
    Send2Gs(GameServiceUpdateSessionMsgId, message, player_node_info->game_node_id());

    if (const auto* const enter_game_node_flag = tls.registry.try_get<EnterGsFlag>(player);
        nullptr != enter_game_node_flag)
    {
        if (const auto enter_gs_type = enter_game_node_flag->enter_gs_type();
            enter_gs_type != LOGIN_NONE)
        {
            PlayerCommonSystem::OnLogin(player);
        }
    }
}

void PlayerCommonSystem::OnLogin(entt::entity player)
{
    const auto enter_game_node_flag = tls.registry.try_get<EnterGsFlag>(player);
	if (nullptr == enter_game_node_flag)
	{
		return;
	}
	if (enter_game_node_flag->enter_gs_type() == LOGIN_FIRST)
	{
	}
	else if (enter_game_node_flag->enter_gs_type() == LOGIN_REPLACE)//顶号
	{
	}
    else if (enter_game_node_flag->enter_gs_type() == LOGIN_RECONNET)//重连
    {
    }

    {
        Centre2GsLoginRequest message;
        message.set_enter_gs_type((*enter_game_node_flag).enter_gs_type());
        tls.registry.remove<EnterGsFlag>(player);
        Send2GsPlayer(GamePlayerServiceCentre2GsLoginMsgId, message, player);
    }

    //给客户端发所有场景消息
    {
        SceneInfoS2C message;
        for (const auto e : tls.registry.view<SceneInfo>())
        {
            message.mutable_scene_info()->Add()->CopyFrom(tls.registry.get<SceneInfo>(e));
        }
        Send2Player(ClientPlayerSceneServicePushSceneInfoS2CMsgId, message, player);
    }
}
