#include "player_common_system.h"

#include "src/game_logic/comp/scene_comp.h"
#include "src/network/message_system.h"

#include "logic_proto/scene_client_player.pb.h"
#include "logic_proto/scene_server_player.pb.h"

PlayerCommonSys g_player_common_sys;

void PlayerCommonSys::OnLogin(entt::entity player)
{

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
