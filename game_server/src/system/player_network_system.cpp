#include "player_network_system.h"

#include "src/comp/player_list.h"
#include "src/network/gate_node.h"
#include "src/network/ms_node.h"
#include "src/network/session.h"

#include "src/pb/pbc/component_proto/player_network_comp.pb.h"

void PlayerNetworkSystem::EnterGs(entt::entity player,  uint64_t session_id, uint64_t ms_node)
{
	registry.emplace_or_replace<GateSession>(player).set_session_id(session_id);
	auto msit = g_ms_nodes.find(ms_node);
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
	registry.emplace_or_replace<GateNodeWPtr>(player, *p_gate);
	//todo进入了gate 然后才可以开始可以给客户端发送信息了, gs消息顺序问题要注意，进入a, 再进入b gs到达客户端消息的顺序不一样
}

