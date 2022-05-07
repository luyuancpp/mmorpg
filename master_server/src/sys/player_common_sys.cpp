#include "player_common_sys.h"

#include "src/game_logic/comp/scene_comp.h"
#include "src/network/message_sys.h"

#include "logic_proto/scene_client_player.pb.h"

PlayerCommonSys g_player_common_sys;

void PlayerCommonSys::OnLogin(entt::entity player)
{
    SceneInfoS2C message;
    for (auto e : reg.view<MainScene>())
    {
        message.mutable_scene_info()->Add()->CopyFrom(reg.get<SceneInfo>(e));
    }
    Send2Player(message, player);
}
