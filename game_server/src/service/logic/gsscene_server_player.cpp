#include "gsscene_server_player.h"
#include "src/game_logic/game_registry.h"
#include "src/network/message_system.h"
///<<< BEGIN WRITING YOUR CODE
#include "muduo/base/Logging.h"

#include "src/comp/player_list.h"
#include "src/game_logic/scene/scene.h"
#include "src/network/gate_node.h"
#include "src/network/message_system.h"
#include "src/network/session.h"
#include "src/system/entity_scene_system.h"

#include "logic_proto/scene_client_player.pb.h"
///<<< END WRITING YOUR CODE

///<<<rpc begin
void ServerPlayerSceneServiceImpl::EnterSceneGs2Ms(entt::entity player,
    const ::Gs2MsEnterSceneRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void ServerPlayerSceneServiceImpl::EnterSceneMs2Gs(entt::entity player,
    const ::Ms2GsEnterSceneRequest* request,
    ::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
    auto scene = ScenesSystem::GetSingleton().get_scene(request->scene_info().scene_id());
    if (scene == entt::null)
    {
        LOG_FATAL << "scene not " << request->scene_info().scene_confid() << "," << request->scene_info().scene_id();
        return;
    }
    auto gate_node_id = node_id(request->session_id());
    auto gate_it = g_gate_nodes.find(gate_node_id);
    if (gate_it != g_gate_nodes.end())
    {
		auto p_gate = registry.try_get<GateNodePtr>(gate_it->second);
		if (nullptr != p_gate)
		{
			registry.emplace_or_replace<GateNodeWPtr>(player, *p_gate);
		}
        else
        {
            LOG_ERROR << " gate not found " << gate_node_id;
        }
    }
    else 
    {
        LOG_ERROR << " gate not found " << gate_node_id;
    }
    


    //todo进入了gate 然后才可以开始可以给客户端发送信息了, gs消息顺序问题要注意，进入a, 再进入b gs到达客户端消息的顺序不一样

    g_entity_scene_system.LeaveScene(player);

    EnterSceneParam ep;
    ep.enterer_ = player;
    ep.scene_ = scene;
    g_entity_scene_system.EnterScene(ep);
///<<< END WRITING YOUR CODE
}

///<<<rpc end
