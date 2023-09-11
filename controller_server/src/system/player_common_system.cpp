#include "player_common_system.h"

#include "muduo/base/Logging.h"

#include "src/game_logic/comp/scene_comp.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/pb/pbc/scene_client_player_service.h"
#include "src/pb/pbc/game_server_player_service.h"
#include "src/network/message_system.h"
#include "src/network/player_session.h"
#include "src/pb/pbc/game_service_service.h"
#include "src/system/player_change_scene.h"

#include "component_proto/player_login_comp.pb.h"
#include "component_proto/player_comp.pb.h"

void PlayerCommonSystem::InitPlayerComponent(entt::entity player, Guid player_id)
{
    controller_tls.player_list().emplace(player_id, player);
    tls.registry.emplace<Guid>(player, player_id);
    tls.registry.emplace<Player>(player);
    PlayerChangeSceneSystem::InitChangeSceneQueue(player);
}

void PlayerCommonSystem::OnEnterGateSucceed(entt::entity player)
{
    const auto* const try_player_session = tls.registry.try_get<PlayerSession>(player);
    if (nullptr == try_player_session)
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
    message.set_session_id(try_player_session->session_id());
    message.set_player_id(*player_id);
    Send2Gs(GameServiceUpdateSessionMsgId, message, try_player_session->gs_node_id());

    if (const auto* const try_enter_gs = tls.registry.try_get<EnterGsFlag>(player);
        nullptr != try_enter_gs)
    {
        if (const auto enter_gs_type = try_enter_gs->enter_gs_type(); enter_gs_type != LOGIN_NONE)
        {
            PlayerCommonSystem::OnLogin(player);
        }
    }
}

void PlayerCommonSystem::OnLogin(entt::entity player)
{
    const auto try_enter_gs = tls.registry.try_get<EnterGsFlag>(player);
	if (nullptr == try_enter_gs)
	{
		return;
	}
	if (try_enter_gs->enter_gs_type() == LOGIN_FIRST)
	{
	}
	else if (try_enter_gs->enter_gs_type() == LOGIN_REPLACE)//顶号
	{
	}
    else if (try_enter_gs->enter_gs_type() == LOGIN_RECONNET)//重连
    {
    }

    {
        Controller2GsLoginRequest message;
        message.set_enter_gs_type((*try_enter_gs).enter_gs_type());
        tls.registry.remove<EnterGsFlag>(player);
        Send2GsPlayer(GamePlayerServiceController2GsLoginMsgId, message, player);
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
