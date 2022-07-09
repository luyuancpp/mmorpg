#include "player_common_system.h"

#include "src/game_logic/comp/scene_comp.h"
#include "src/network/message_system.h"
#include "src/network/player_session.h"

#include "logic_proto/scene_client_player.pb.h"
#include "logic_proto/login_server_player.pb.h"
#include "component_proto/player_login_comp.pb.h"

void PlayerCommonSystem::OnLogin(entt::entity player)
{
	auto try_enter_gs = registry.try_get<EnterGsFlag>(player);
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
        Ms2GsLoginRequest message;
        auto try_enter_gs = registry.try_get<EnterGsFlag>(player);
        if (nullptr != try_enter_gs)
        {
            message.set_enter_gs_type((*try_enter_gs).enter_gs_type());
            registry.remove<EnterGsFlag>(player);
        }
        auto try_player_session = registry.try_get<PlayerSession>(player);
        if (nullptr == try_player_session)
        {
            LOG_ERROR << "player session not valid" << registry.try_get<Guid>(player);
            return;
        }
        message.set_session_id(try_player_session->session_id());
        Send2GsPlayer(message, player);
    }
   
    //给客户端发所有场景消息
    {
        SceneInfoS2C message;
        for (auto e : registry.view<MainScene>())
        {
            message.mutable_scene_info()->Add()->CopyFrom(registry.get<SceneInfo>(e));
        }
        Send2Player(message, player);
    }    
}
