#include "player_scene_sys.h"

#include "src/network/message_sys.h"

#include "gs_node.pb.h"
#include "logic_proto/scene_client_player.pb.h"

PlayerSceneSystem g_player_scene_system;

void PlayerSceneSystem::OnEnterScene(entt::entity player)
{
    EnterSeceneS2C msg;//������gate Ȼ��ſ��Կ�ʼ���Ը��ͻ��˷�����Ϣ��
    Send2Player(msg, player);
}