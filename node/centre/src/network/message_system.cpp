#include "message_system.h"

#include "muduo/base/Logging.h"

#include "src/type_define/type_define.h"
#include "src/network/session.h"
#include "src/comp/scene_comp.h"
#include "src/thread_local/thread_local_storage.h"
#include "src/comp/player_list.h"
#include "src/network/game_node.h"
#include "src/network/gate_node.h"
#include "src/network/server_component.h"
#include "service/service.h"
#include "service/gate_service_service.h"
#include "service/game_service_service.h"
#include "component_proto/player_network_comp.pb.h"
#include "src/thread_local/centre_thread_local_storage.h"


void Send2Gs(uint32_t message_id, const google::protobuf::Message& message, uint32_t node_id)
{
	auto it = centre_tls.game_node().find(node_id);
	if (it == centre_tls.game_node().end())
	{
        LOG_ERROR << "gs not found ->" << node_id;
		return;
	}
	auto node =  tls.registry.try_get<GameNodePtr>(it->second);
	if (nullptr == node)
	{
		LOG_ERROR << "gs not found ->" << node_id;
		return;
	}

	(*node)->session_.Send(message_id, message);
}

void Send2GsPlayer(const uint32_t message_id, const google::protobuf::Message& message, entt::entity player)
{
	if (!tls.registry.valid(player))
	{
		return;
	}
	const auto* player_node_info = tls.registry.try_get<PlayerNodeInfo>(player);
	if (nullptr == player_node_info)
	{
		return;
	}
	const auto game_node_it = centre_tls.game_node().find(player_node_info->game_node_id());
	if (centre_tls.game_node().end() == game_node_it)
	{
		return;
	}
	NodeRouteMessageRequest message_wrapper;
	message_wrapper.mutable_msg()->mutable_body()->resize(message.ByteSizeLong());
	message.SerializePartialToArray(message_wrapper.mutable_msg()->mutable_body()->data(), static_cast<int32_t>(message.ByteSizeLong()));
	message_wrapper.mutable_msg()->set_message_id(message_id);
	message_wrapper.mutable_ex()->set_session_id(player_node_info->gate_session_id());
	tls.registry.get<GameNodePtr>(game_node_it->second)->session_.Send(GameServiceSend2PlayerMsgId, message_wrapper);
}


void Send2GsPlayer(uint32_t message_id, const google::protobuf::Message& message, Guid player_id)
{
    Send2GsPlayer(message_id, message, ControllerPlayerSystem::GetPlayer(player_id));
}

void Send2PlayerViaGs(uint32_t message_id, const google::protobuf::Message& message, Guid player_id)
{
	Send2PlayerViaGs(message_id, message, ControllerPlayerSystem::GetPlayer(player_id));
}

void Send2PlayerViaGs(uint32_t message_id, const google::protobuf::Message& message, entt::entity player)
{
	if (!tls.registry.valid(player))
	{
		return;
	}
	const auto* player_node_info = tls.registry.try_get<PlayerNodeInfo>(player);
	if (nullptr == player_node_info)
	{
		return;
	}
	const auto game_node_it = centre_tls.game_node().find(player_node_info->game_node_id());
	if (centre_tls.game_node().end() == game_node_it)
	{
		return;
	}
	NodeRouteMessageRequest message_wrapper;
	auto byte_size = int32_t(message.ByteSizeLong());
	message_wrapper.mutable_msg()->mutable_body()->resize(byte_size);
	message.SerializePartialToArray(message_wrapper.mutable_msg()->mutable_body()->data(), byte_size);
	message_wrapper.mutable_ex()->set_session_id(player_node_info->gate_session_id());
	tls.registry.get<GameNodePtr>(game_node_it->second)->session_.Send(message_id, message_wrapper);
}

void Send2Player(uint32_t message_id, const google::protobuf::Message& message, entt::entity player)
{
	if (!tls.registry.valid(player))
	{
		return;
	}
	const auto* player_node_info = tls.registry.try_get<PlayerNodeInfo>(player);
	if (nullptr == player_node_info)
	{
		return;
	}
	const auto gate_it = centre_tls.gate_nodes().find(get_gate_node_id(player_node_info->gate_session_id()));
	if (gate_it == centre_tls.gate_nodes().end())
	{
		LOG_ERROR << "gate not found ";
		return;
	}
	Send2Player(message_id, message, gate_it->second, player_node_info->gate_session_id());
}

void Send2Player(uint32_t message_id, const google::protobuf::Message& message, GateNodePtr& gate, uint64_t session_id)
{
	NodeRouteMessageRequest message_wrapper;
	const auto byte_size = static_cast<int32_t>(message.ByteSizeLong());
	message_wrapper.mutable_msg()->mutable_body()->resize(byte_size);
	message.SerializePartialToArray(message_wrapper.mutable_msg()->mutable_body()->data(), byte_size);
	message_wrapper.mutable_ex()->set_session_id(session_id);
	message_wrapper.mutable_msg()->set_message_id(message_id);
	gate->session_.Send(GateServicePlayerMessageMsgId, message_wrapper);
}

void Send2Player(uint32_t message_id, const google::protobuf::Message& message, Guid player_id)
{
	const auto player = ControllerPlayerSystem::GetPlayer(player_id);
	Send2Player(message_id, message, player);
}

void Send2Gate(const uint32_t message_id, const google::protobuf::Message& message, uint32_t gate_id)
{
	const auto gate_it = centre_tls.gate_nodes().find(gate_id);
	if (gate_it == centre_tls.gate_nodes().end())
	{
		return;
	}
	gate_it->second->session_.Send(message_id, message);
}

void CallGamePlayerMethod(uint32_t message_id, const google::protobuf::Message& message, entt::entity player)
{
	if (!tls.registry.valid(player))
	{
		return;
	}
	const auto* player_node_info = tls.registry.try_get<PlayerNodeInfo>(player);
	if (nullptr == player_node_info)
	{
		return;
	}
	const auto game_node_it = centre_tls.game_node().find(player_node_info->game_node_id());
	if (centre_tls.game_node().end() == game_node_it)
	{
		return;
	}
	NodeRouteMessageRequest message_wrapper;
	const auto byte_size = static_cast<int32_t>(message.ByteSizeLong());
	message_wrapper.mutable_msg()->mutable_body()->resize(byte_size);
	message.SerializePartialToArray(message_wrapper.mutable_msg()->mutable_body()->data(), byte_size);
	message_wrapper.mutable_msg()->set_message_id(message_id);
	message_wrapper.mutable_ex()->set_session_id(player_node_info->gate_session_id());
	tls.registry.get<GameNodePtr>(game_node_it->second)->session_.CallMethod(GameServiceCallPlayerMsgId, message_wrapper);
}

bool CallGameNodeMethod(uint32_t message_id, const google::protobuf::Message& message, NodeId node_id)
{
	const auto it = centre_tls.game_node().find(node_id);
	if (it == centre_tls.game_node().end())
	{
		return false;
	}
	tls.registry.get<GameNodePtr>(it->second)->session_.CallMethod(message_id, message);
	return true;
}