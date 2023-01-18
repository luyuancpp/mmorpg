#include "player_common_system.h"

#include "muduo/base/Logging.h"

#include "src/game_logic/comp/scene_comp.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/network/message_system.h"
#include "src/network/player_session.h"
#include "src/system/player_change_scene.h"

#include "logic_proto/scene_client_player.pb.h"
#include "logic_proto/common_server_player.pb.h"
#include "component_proto/player_login_comp.pb.h"
#include "component_proto/player_comp.pb.h"

void PlayerCommonSystem::InitPlayerCompnent(entt::entity player)
{
    PlayerChangeSceneSystem::InitChangeSceneQueue(player);
    tls.registry.emplace<Player>(player);
}

void PlayerCommonSystem::OnEnterGateSucceed(entt::entity player)
{
    UpdateSessionController2GsRequest message;
    auto try_player_session = tls.registry.try_get<PlayerSession>(player);
    if (nullptr == try_player_session)
    {
        LOG_ERROR << "player session not valid" << tls.registry.try_get<Guid>(player);
        return;
    }
    message.set_session_id(try_player_session->session_id());
    Send2GsPlayer(message, player);

    auto try_enter_gs = tls.registry.try_get<EnterGsFlag>(player);
    if (nullptr != try_enter_gs)
    {
        auto enter_gs_type = try_enter_gs->enter_gs_type();
        if (enter_gs_type != LOGIN_NONE)
        {
            PlayerCommonSystem::OnLogin(player);
        }
    }
}

void PlayerCommonSystem::OnLogin(entt::entity player)
{
	auto try_enter_gs = tls.registry.try_get<EnterGsFlag>(player);
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
        Send2GsPlayer(message, player);
    }
   
    //给客户端发所有场景消息
    {
        SceneInfoS2C message;
        for (auto e : tls.registry.view<MainScene>())
        {
            message.mutable_scene_info()->Add()->CopyFrom(tls.registry.get<SceneInfo>(e));
        }
        Send2Player(message, player);
    }    
}
