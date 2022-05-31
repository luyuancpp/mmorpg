#include "player_scene_system.h"

#include "src/comp/player_list.h"
#include "src/game_logic/scene/scene.h"
#include "src/network/gate_node.h"
#include "src/network/ms_node.h"
#include "src/network/session.h"

#include "src/pb/pbc/component_proto/player_async_comp.pb.h"
#include "src/pb/pbc/component_proto/player_network_comp.pb.h"

void PlayerSceneSystem::EnterScene(entt::entity player, const EnterSceneInfo& enter_info, uint64_t session_id)
{
	//第一次进入世界,但是gate还没进入
	auto scene = ScenesSystem::GetSingleton().get_scene(enter_info.scenes_info().scene_id());
	if (scene == entt::null)
	{
		LOG_ERROR << "scene not found " << enter_info.scenes_info().scene_id() <<  "," << enter_info.scenes_info().scene_confid();
		return;
	}
	registry.emplace<GateSession>(player).set_session_id(session_id);

	auto msit = g_ms_nodes.find(enter_info.ms_node_id());
	if (msit != g_ms_nodes.end())
	{
		registry.emplace_or_replace<MsNodeWPtr>(player, msit->second);
	}
	auto gate_node_id = node_id(session_id);
	auto gate_it = g_gate_nodes.find(gate_node_id);
	if (gate_it == g_gate_nodes.end())
	{
		LOG_ERROR << " gate not found" << gate_node_id;
		return;
	}
	auto p_gate = registry.try_get<GateNodePtr>(gate_it->second);
	if (nullptr == p_gate)
	{
		LOG_ERROR << " gate not found" << gate_node_id;
		return;
	}
	registry.emplace<GateNodeWPtr>(player, *p_gate);
	EnterSceneParam ep;
	ep.enterer_ = player;
	ep.scene_ = scene;
	ScenesSystem::GetSingleton().EnterScene(ep);

	//todo进入了gate 然后才可以开始可以给客户端发送信息了, gs消息顺序问题要注意，进入a, 再进入b gs到达客户端消息的顺序不一样

}

