#include "player_node.h"

#include "muduo/base/Logging.h"

#include "comp/scene.h"
#include "thread_local/storage.h"

#include "network/gate_session.h"
#include "network/message_system.h"
#include "service/game_server_player_service.h"
#include "service/game_service_service.h"
#include "service/gate_service_service.h"
#include "system/player_change_scene.h"
#include "system/player_scene.h"
#include "system/scene/scene_system.h"
#include "thread_local/storage_centre.h"
#include "thread_local/storage_common_logic.h"
#include "type_alias/player_loading.h"
#include "util/defer.h"

#include "proto/logic/component/player_comp.pb.h"
#include "proto/logic/component/player_login_comp.pb.h"
#include "proto/logic/component/player_network_comp.pb.h"

void PlayerNodeSystem::OnPlayerAsyncLoaded(Guid player_id, const player_centre_database& message)
{
    auto& loading_list = tls.globalRegistry.get<PlayerLoadingInfoList>(global_entity());
    defer(loading_list.erase(player_id));
    const auto it = loading_list.find(player_id);
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
    tls.registry.emplace<PlayerSceneInfoComp>(player, message.scene_info());
    PlayerChangeSceneSystem::InitChangeSceneQueue(player);
    //第一次登录
    tls.registry.emplace<EnterGsFlag>(player).set_enter_gs_type(LOGIN_FIRST);
    PlayerSceneSystem::OnLoginEnterScene(player);
    // on loaded db
}

void PlayerNodeSystem::OnPlayerAsyncSaved(Guid player_id, player_centre_database& message)
{

}

void PlayerNodeSystem::OnLogin(entt::entity player)
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
        message.set_enter_gs_type(enter_game_node_flag->enter_gs_type());
        tls.registry.remove<EnterGsFlag>(player);
        SendToGsPlayer(GamePlayerServiceCentre2GsLoginMsgId, message, player);
    }
}

void PlayerNodeSystem::Register2GatePlayerGameNode(entt::entity player)
{
    auto* player_node_info = tls.registry.try_get<PlayerNodeInfo>(player);
    if (nullptr == player_node_info)
    {
        LOG_ERROR << "player session not found" << tls.registry.try_get<Guid>(player);
        return;
    }
    entt::entity gate_node_id{ GetGateNodeId(player_node_info->gate_session_id()) };
    if (!tls.gateNodeRegistry.valid(gate_node_id))
    {
        LOG_ERROR << "gate crash" << GetGateNodeId(player_node_info->gate_session_id());
        return;
    }
    auto gate_node = tls.gateNodeRegistry.try_get<RpcSessionPtr>(gate_node_id);
    if (nullptr == gate_node)
    {
        LOG_ERROR << "gate crash" << GetGateNodeId(player_node_info->gate_session_id());
        return;
    }

    RegisterSessionGameNodeRequest rq;
    rq.mutable_session_info()->set_session_id(player_node_info->gate_session_id());
    rq.set_game_node_id(player_node_info->game_node_id());
    (*gate_node)->CallMethod(GateServicePlayerEnterGsMsgId, rq);
}


void PlayerNodeSystem::OnRegister2GatePlayerGameNode(entt::entity player)
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
    SendToGs(GameServiceUpdateSessionMsgId, rq, player_node_info->game_node_id());

    if (const auto* const enter_game_node_flag = tls.registry.try_get<EnterGsFlag>(player);
        nullptr != enter_game_node_flag)
    {
        if (const auto enter_gs_type = enter_game_node_flag->enter_gs_type();
            enter_gs_type != LOGIN_NONE)
        {
            PlayerNodeSystem::OnLogin(player);
        }
    }
}

void PlayerNodeSystem::LeaveGame(Guid player_uid)
{
    //todo 登录的时候leave
    //todo 断线不能马上下线，这里之后会改
    //没进入场景，只是登录，或者切换场景过程中
    defer(tls_cl.player_list().erase(player_uid));
    const auto player = tls_cl.get_player(player_uid);
    if (!tls.registry.valid(player))
    {
        return;
    }
    if (nullptr == tls.registry.try_get<SceneEntity>(player))
    {
    }
    else
    {
        ScenesSystem::LeaveScene({ player });
    }
}

