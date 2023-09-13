#include "game_server_player_handler.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/network/message_system.h"
///<<< BEGIN WRITING YOUR CODE

#include "muduo/base/Logging.h"

#include "src/game_logic/thread_local/common_logic_thread_local_storage.h"
#include "src/network/session.h"
#include "src/system/player_common_system.h"
#include "src/thread_local/game_thread_local_storage.h"

#include "component_proto/player_login_comp.pb.h"
#include "component_proto/player_network_comp.pb.h"
///<<< END WRITING YOUR CODE
void GamePlayerServiceHandler::UpdateSessionController2Gs(entt::entity player,
	const ::UpdateSessionController2GsRequest* request,
	::google::protobuf::Empty* response)
{
	///<<< BEGIN WRITING YOUR CODE
	PlayerCommonSystem::RemovePlayerSession(tls.registry.get<Guid>(player));
	const auto gate_node_id = get_gate_node_id(request->session_id());
	//test
	if (const auto gate_it = game_tls.gate_node().find(gate_node_id);
		gate_it == game_tls.gate_node().end())
	{
		LOG_ERROR << "EnterSceneMs2Gs gate not found " << gate_node_id;
		return;
	}
	game_tls.gate_sessions().emplace(request->session_id(), player);
	//登录更新gate
	if (auto* const player_node_info = tls.registry.try_get<PlayerNodeInfo>(player);
		nullptr == player_node_info)
	{
		//登录更新gate
		tls.registry.emplace_or_replace<PlayerNodeInfo>(player).set_gate_session_id(request->session_id());
	}
	else
	{
		player_node_info->set_gate_session_id(request->session_id());
	}
	///<<< END WRITING YOUR CODE
}

void GamePlayerServiceHandler::Controller2GsLogin(entt::entity player,
	const ::Controller2GsLoginRequest* request,
	::google::protobuf::Empty* response)
{
	///<<< BEGIN WRITING YOUR CODE
	if (request->enter_gs_type() == LOGIN_NONE)//登录，不是普通切换场景
	{
		return;
	}
	PlayerCommonSystem::OnPlayerLogin(player, request->enter_gs_type());
	///<<< END WRITING YOUR CODE
}

void GamePlayerServiceHandler::OnControllerEnterGateSucceed(entt::entity player,
	const ::Controller2GsEnterGateSucceedRequest* request,
	::google::protobuf::Empty* response)
{
	///<<< BEGIN WRITING YOUR CODE
	PlayerCommonSystem::OnEnterGateUpdateGameNodeSucceed(player);
	///<<< END WRITING YOUR CODE
}

