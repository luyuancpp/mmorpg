#include "message_system.h"

#include "muduo/base/Logging.h"

#include "src/game_logic/player/player_list.h"
#include "src/game_logic/thread_local/thread_local_storage.h"
#include "src/network/gate_node.h"
#include "src/network/controller_node.h"
#include "src/pb/pbc/controller_service_service.h"
#include "src/pb/pbc/gate_service_service.h"
#include "src/thread_local/game_thread_local_storage.h"

#include "gate_service.pb.h"
#include "controller_service.pb.h"
#include "component_proto/player_network_comp.pb.h"

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
	auto try_gate = tls.registry.try_get<GateNodeWPtr>(player);
	if (nullptr == try_gate)
	{
		LOG_ERROR << "Send2Player player gate not found " << tls.registry.get<Guid>(player);
		return;
	}

	auto gate = (*try_gate).lock();
	if (nullptr == gate)
	{
		LOG_INFO << "Send2Player player gate not found " << tls.registry.get<Guid>(player);
		return;
	}
	NodeServiceMessageRequest message_wrapper;
	message_wrapper.mutable_msg()->set_message_id(message_id);
	message_wrapper.mutable_msg()->set_body(message.SerializeAsString());
	message_wrapper.mutable_ex()->set_session_id(tls.registry.get<GateSession>(player).session_id());
	gate->session_.Send(GateServicePlayerMessageMsgId, message_wrapper);
}

void Send2Player(uint32_t message_id, const google::protobuf::Message& msg, EntityPtr& player)
{
    Send2Player(message_id, msg, (entt::entity)player);
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
	auto controller_node = tls.registry.get<ControllerNodePtr>(player);
	if (nullptr == controller_node)
	{
		LOG_ERROR << "Send2ControllerPlayer player controller not found " << tls.registry.get<Guid>(player);
		return;
	}
	if (!controller_node->session_->connected())
	{
		LOG_ERROR << "Send2ControllerPlayer controller disconnect" << tls.registry.get<Guid>(player);
		return;
	}
	NodeServiceMessageRequest msg_wrapper;
	msg_wrapper.mutable_msg()->set_message_id(message_id);
	msg_wrapper.mutable_msg()->set_body(msg.SerializeAsString());
	msg_wrapper.mutable_ex()->set_player_id(tls.registry.get<Guid>(player));
	controller_node->session_->Send(ControllerServiceGsPlayerServiceMsgId, msg_wrapper);
}

void Send2ControllerPlayer(uint32_t message_id, const google::protobuf::Message& message, EntityPtr& player)
{
	Send2ControllerPlayer(message_id, message, (entt::entity)player);
}

void Send2Controller(uint32_t message_id, const google::protobuf::Message& messag, uint32_t controller_node_id)
{
	auto controller_it = game_tls.controller_node().find(controller_node_id);
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

