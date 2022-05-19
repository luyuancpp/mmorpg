#include "player_scene_system.h"

#include "muduo/base/Logging.h"

#include "src/game_logic/comp/scene_comp.h"
#include "src/network/message_system.h"
#include "src/network/player_session.h"

#include "gs_service.pb.h"
#include "logic_proto/scene_server_player.pb.h"

PlayerSceneSystem g_player_scene_system;

void PlayerSceneSystem::OnEnterScene(entt::entity player)
{
    auto p_scene = reg.try_get<SceneEntity>(player);
    auto player_id = reg.get<Guid>(player);
    if (nullptr == p_scene)
    {
        LOG_ERROR << "player do not enter scene " << player_id;
        return;
    }
    Ms2GsEnterSceneRequest message;
    auto p_scene_info = reg.try_get<SceneInfo>((*p_scene).scene_entity());
    if (nullptr == p_scene_info)
    {
        LOG_ERROR << "scene info " << player_id;
        return;
    }
    message.mutable_scene_info()->CopyFrom(*p_scene_info);
    Send2Gs(message, reg.get<PlayerSession>(player).gs_node_id());
}