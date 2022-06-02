#include "player_common_system.h"

#include "src/game_logic/comp/scene_comp.h"
#include "src/network/message_system.h"

#include "logic_proto/scene_client_player.pb.h"
#include "logic_proto/scene_server_player.pb.h"
#include "component_proto/player_login_comp.pb.h"

void PlayerCommonSystem::OnLogin(entt::entity player)
{
	auto try_enter_gs = registry.try_get<EnterGsComp>(player);
	if (nullptr != try_enter_gs)
	{
		if (try_enter_gs->enter_gs_type() == LOGIN_FIRST)
		{
		}
		else if (try_enter_gs->enter_gs_type() == LOGIN_REPLACE)//顶号
		{
		}
		else if (try_enter_gs->enter_gs_type() == LOGIN_RECONNET)//重连
		{
		}
	}
    //给客户端发所有场景消息
    {
        SceneInfoS2C message;
        for (auto e : registry.view<MainScene>())
        {
            message.mutable_scene_info()->Add()->CopyFrom(registry.get<SceneInfo>(e));
        }
        Send2PlayerViaGs(message, player);
    }    
}
