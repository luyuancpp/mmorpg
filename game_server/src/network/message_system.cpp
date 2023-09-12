#include "message_system.h"

#include "muduo/base/Logging.h"

#include "src/game_logic/thread_local/common_logic_thread_local_storage.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/network/gate_node.h"
#include "src/network/controller_node.h"
#include "src/network/session.h"
#include "src/pb/pbc/controller_service_service.h"
#include "src/pb/pbc/gate_service_service.h"
#include "src/thread_local/game_thread_local_storage.h"

#include "component_proto/player_network_comp.pb.h"
#include "src/game_logic/comp/player_comp.h"

void Send2Player(uint32_t message_id, const google::protobuf::Message& message, Guid player_id)
{
	auto it = game_tls.player_list().find(player_id);
	if (it == game_tls.player_list().end())
	{
		LOG_DEBUG << "Send2Player player not found " << player_id;
		return;
	}
	Send2Player(message_id, message, it->second);
}

void Send2Player(uint32_t message_id, const google::protobuf::Message& message, entt::entity player)
{
	if (!tls.registry.valid(player))
	{
		return;
	}
	const auto try_gate_session = tls.registry.try_get<GateSession>(player);
	if (nullptr == try_gate_session)
	{
		LOG_INFO << "player gate session not found " << tls.registry.get<Guid>(player);
		return;
	}
	const auto gate_it = game_tls.gate_node().find(node_id((*try_gate_session).session_id()));
	if (game_tls.gate_node().end() == gate_it)
	{
		LOG_INFO << "gate not found " << node_id((*try_gate_session).session_id());
		return;
	}
	NodeServiceMessageRequest message_wrapper;
	message_wrapper.mutable_msg()->set_message_id(message_id);
	message_wrapper.mutable_msg()->set_body(message.SerializeAsString());
	message_wrapper.mutable_ex()->set_session_id((*try_gate_session).session_id());
	gate_it->second->session_.Send(GateServicePlayerMessageMsgId, message_wrapper);
}

void Send2ControllerPlayer(uint32_t message_id, const google::protobuf::Message& message, Guid player_id)
{
	auto it = game_tls.player_list().find(player_id);
	if (it == game_tls.player_list().end())
	{
		LOG_DEBUG << " Send2ControllerPlayer player not found " << player_id;
		return;
	}
	Send2ControllerPlayer(message_id, message, it->second);
}

void Send2ControllerPlayer(uint32_t message_id, const google::protobuf::Message& msg, entt::entity player)
{
	if (!tls.registry.valid(player))
	{
		return;
	}
	const auto* const try_player_node_info = tls.registry.try_get<PlayerNodeInfo>(player);
	if (nullptr == try_player_node_info)
	{
		LOG_ERROR << "player node info  not found" << tls.registry.get<Guid>(player);
		return;
	}
	const auto controller_it = game_tls.controller_node().find(try_player_node_info->controller_node_id_);
	if (controller_it == game_tls.controller_node().end())
	{
		LOG_ERROR << "controller not found" << try_player_node_info->controller_node_id_;
		return;
	}
	if (!controller_it->second->session_->connected())
	{
		LOG_ERROR << "Send2ControllerPlayer controller disconnect" << tls.registry.get<Guid>(player);
		return;
	}
	const auto* const try_gate_session = tls.registry.try_get<GateSession>(player);
	if (nullptr == try_gate_session)
	{
		LOG_INFO << "Send2Player player gate not found " << tls.registry.get<Guid>(player);
		return;
	}
	NodeServiceMessageRequest msg_wrapper;
	msg_wrapper.mutable_msg()->set_message_id(message_id);
	msg_wrapper.mutable_msg()->set_body(msg.SerializeAsString());
	msg_wrapper.mutable_ex()->set_session_id(try_gate_session->session_id());
	controller_it->second->session_->Send(ControllerServiceGsPlayerServiceMsgId, msg_wrapper);
}

void Send2Controller(const uint32_t message_id, const google::protobuf::Message& messag, uint32_t controller_node_id)
{
	const auto controller_it = game_tls.controller_node().find(controller_node_id);
	if (controller_it == game_tls.controller_node().end())
	{
		LOG_ERROR << "Send2ControllerPlayer controller not found" << controller_node_id;
		return;
	}
	controller_it->second->session_->Send(message_id, messag);
}

void Send2Gate(uint32_t message_id, const google::protobuf::Message& messag, uint32_t gate_node_id)
{
	auto gate_it = game_tls.gate_node().find(gate_node_id);
	if (gate_it == game_tls.gate_node().end())
	{
		LOG_ERROR << "Send2Gate gate not found" << gate_node_id;
		return;
	}
	gate_it->second->session_.Send(GateServicePlayerMessageMsgId, messag);
}

bool CallGameNodeMethod(const uint32_t message_id, const google::protobuf::Message& message, const NodeId node_id)
{
	const auto controller_it = game_tls.controller_node().find(node_id);
	if (controller_it == game_tls.controller_node().end())
	{
		LOG_ERROR << "controller not found" << node_id;
		return false;
	}
	controller_it->second->session_->CallMethod(message_id, message);
	return true;
}