#include "player_common_system.h"

#include "muduo/base/Logging.h"

#include "comp/scene_comp.h"
#include "thread_local/thread_local_storage.h"

#include "service/game_server_player_service.h"
#include "network/message_system.h"
#include "network/gate_session.h"
#include "service/game_service_service.h"
#include "system/player_change_scene.h"
#include "thread_local/thread_local_storage_centre.h"
#include "thread_local/thread_local_storage_common_logic.h"
#include "util/defer.h"
#include "system/player_scene_system.h"
#include "type_alias/player_loading.h"
#include "service/gate_service_service.h"

#include "component_proto/player_login_comp.pb.h"
#include "component_proto/player_comp.pb.h"
#include "component_proto/player_network_comp.pb.h"

void PlayerCommonSystem::OnPlayerAsyncLoaded(Guid player_id, const player_centre_database& message)
{
    auto& loading_list = tls.global_registry.get<PlayerLoadingInfoList>(global_entity());
    defer(loading_list.erase(player_id));
    auto it = loading_list.find(player_id);
    if ( it == loading_list.end() )
    {
        LOG_ERROR << "loading player  error" << player_id;
        return;
    }
    auto player = tls.registry.create();
    if (const auto [fst, snd] = tls_cl.player_list().emplace(player_id, player); !snd)
    {
        LOG_ERROR << "server emplace error" << player_id;
        return;
    }
 
    tls.registry.emplace_or_replace<PlayerNodeInfo>(player).set_gate_session_id(
        it->second.session_info().session_id());

    tls.registry.emplace<Player>(player);
    tls.registry.emplace<Guid>(player, player_id);
    tls.registry.emplace<PlayerSceneInfoComp>(player, std::move(message.scene_info()));
    PlayerChangeSceneSystem::InitChangeSceneQueue(player);
    //第一次登录
    tls.registry.emplace<EnterGsFlag>(player).set_enter_gs_type(LOGIN_FIRST);
    PlayerSceneSystem::OnLoginEnterScene(player);
    // on loaded db
}

void PlayerCommonSystem::OnPlayerAsyncSaved(Guid player_id, player_centre_database& message)
{

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
}

void PlayerCommonSystem::Register2GatePlayerGameNode(entt::entity player)
{
    auto* player_node_info = tls.registry.try_get<PlayerNodeInfo>(player);
    if (nullptr == player_node_info)
    {
        LOG_ERROR << "player session not found" << tls.registry.try_get<Guid>(player);
        return;
    }
    entt::entity gate_node_id{ get_gate_node_id(player_node_info->gate_session_id()) };
    if (!tls.gate_node_registry.valid(gate_node_id))
    {
        LOG_ERROR << "gate crash" << get_gate_node_id(player_node_info->gate_session_id());
        return;
    }
    auto gate_node = tls.gate_node_registry.try_get<RpcSessionPtr>(gate_node_id);
    if (nullptr == gate_node)
    {
        LOG_ERROR << "gate crash" << get_gate_node_id(player_node_info->gate_session_id());
        return;
    }

    RegisterSessionGameNodeRequest rq;
    rq.mutable_session_info()->set_session_id(player_node_info->gate_session_id());
    rq.set_game_node_id(player_node_info->game_node_id());
    (*gate_node)->CallMethod(GateServicePlayerEnterGsMsgId, rq);
}


void PlayerCommonSystem::OnRegister2GatePlayerGameNode(entt::entity player)
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
    RegisterPlayerSessionRequest rq;
    rq.set_session_id(player_node_info->gate_session_id());
    rq.set_player_id(*player_id);
    Send2Gs(GameServiceUpdateSessionMsgId, rq, player_node_info->game_node_id());

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
