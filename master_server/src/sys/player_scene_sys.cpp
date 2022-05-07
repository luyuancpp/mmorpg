#include "player_scene_sys.h"

#include "muduo/base/Logging.h"

#include "src/game_logic/comp/gs_scene_comp.h"
#include "src/network/message_sys.h"

#include "gs_node.pb.h"
#include "logic_proto/scene_client_player.pb.h"

PlayerSceneSystem g_player_scene_system;

void PlayerSceneSystem::OnEnterScene(entt::entity player)
{
    auto p_scene = reg.try_get<SceneEntity>(player);
    if (nullptr == p_scene)
    {
        LOG_ERROR << "scene error" << reg.get<Guid>(player);
        return;
    }
    EnterSeceneS2C msg;//������gate Ȼ��ſ��Կ�ʼ���Ը��ͻ��˷�����Ϣ��
    Send2Player(msg, player);
}