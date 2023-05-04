#include "common_server_player_handler.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/network/message_system.h"
///<<< BEGIN WRITING YOUR CODE
#include "src/network/session.h"
#include "src/system/player_common_system.h"
#include "src/thread_local/game_thread_local_storage.h"

#include "component_proto/player_network_comp.pb.h"
///<<< END WRITING YOUR CODE
void ServerPlayerLoginServiceHandler::UpdateSessionController2Gs(entt::entity player,
	const ::UpdateSessionController2GsRequest* request,
	::google::protobuf::Empty* response)
{
///<<< BEGIN WRITING YOUR CODE
	PlayerCommonSystem::RemovePlayereSession(tls.registry.get<Guid>(player));
	auto gate_node_id = node_id(request->session_id());
	auto gate_it = game_tls.gate_node().find(gate_node_id);
	if (gate_it == game_tls.gate_node().end())//test
	{
		LOG_ERROR << "EnterSceneMs2Gs gate not found " << gate_node_id;
		return;
	}
	auto p_gate = tls.registry.try_get<GateNodePtr>(gate_it->second);
	if (nullptr == p_gate)
	{
		LOG_ERROR << "EnterSceneMs2Gs gate not found " << gate_node_id;
		return;
	}
	game_tls.gate_sessions().emplace(request->session_id(), player);
	tls.registry.emplace_or_replace<GateSession>(player).set_session_id(request->session_id());//��¼����gate
	tls.registry.emplace_or_replace<GateNodeWPtr>(player, *p_gate);
///<<< END WRITING YOUR CODE
}
