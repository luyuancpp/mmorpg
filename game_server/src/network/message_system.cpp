#include "message_system.h"

#include "muduo/base/Logging.h"

#include "src/game_logic/game_registry.h"
#include "src/game_logic/player/player_list.h"
#include "src/network/gate_node.h"
#include "src/network/controller_node.h"
#include "src/pb/pbc/msgmap.h"

#include "gw_service.pb.h"
#include "controller_service.pb.h"
#include "component_proto/player_network_comp.pb.h"

void Send2Player(const google::protobuf::Message& message, Guid player_id)
{
	auto it = g_players->find(player_id);
	if (it == g_players->end())
	{
		LOG_DEBUG << "Send2Player player not found " << player_id;
		return;
	}
	Send2Player(message, it->second);
}

void Send2Player(const google::protobuf::Message& message, entt::entity player)
{
	if (!registry.valid(player))
	{
		return;
	}
	auto try_gate = registry.try_get<GateNodeWPtr>(player);
	if (nullptr == try_gate)
	{
		LOG_ERROR << "Send2Player player gate not found " << registry.get<Guid>(player);
		return;
	}
	auto message_it = g_msgid.find(message.GetDescriptor()->full_name());
	if (message_it == g_msgid.end())
	{
		LOG_ERROR << "Send2Player message id not found " << message.GetDescriptor()->full_name();
		return;
	}
	auto gate = (*try_gate).lock();
	if (nullptr == gate)
	{
		LOG_INFO << "Send2Player player gate not found " << registry.get<Guid>(player);
		return;
	}
	gwservice::PlayerMessageRequest msg_wrapper;
	msg_wrapper.mutable_msg()->set_msg_id(message_it->second);
	msg_wrapper.mutable_msg()->set_body(message.SerializeAsString());
	msg_wrapper.mutable_ex()->set_session_id(registry.get<GateSession>(player).session_id());
	gate->session_.Send(msg_wrapper);
}

void Send2Player(const google::protobuf::Message& message, EntityPtr& player)
{
    Send2Player(message, (entt::entity)player);
}

void Send2ControllerPlayer(const google::protobuf::Message& message, Guid player_id)
{
	auto it = g_players->find(player_id);
	if (it == g_players->end())
	{
		LOG_DEBUG << " Send2ControllerPlayer player not found " << player_id;
		return;
	}
	Send2ControllerPlayer(message, it->second);
}

void Send2ControllerPlayer(const google::protobuf::Message& message, entt::entity player)
{
	if (!registry.valid(player))
	{
		return;
	}
	auto message_it = g_msgid.find(message.GetDescriptor()->full_name());
	if (message_it == g_msgid.end())
	{
		LOG_ERROR << " Send2ControllerPlayer message id not found " << message.GetDescriptor()->full_name();
		return;
	}
	auto ms_node = registry.get<ControllerNodePtr>(player);
	if (nullptr == ms_node)
	{
		LOG_ERROR << "Send2ControllerPlayer player controller not found " << registry.get<Guid>(player);
		return;
	}
	if (!ms_node->session_->connected())
	{
		LOG_ERROR << "Send2ControllerPlayer controller disconnect" << registry.get<Guid>(player);
		return;
	}
	controllerservice::PlayerNodeServiceRequest msg_wrapper;
	msg_wrapper.mutable_msg()->set_msg_id(message_it->second);
	msg_wrapper.mutable_msg()->set_body(message.SerializeAsString());
	msg_wrapper.mutable_ex()->set_player_id(registry.get<Guid>(player));
	ms_node->session_->Send(msg_wrapper);
}

void Send2ControllerPlayer(const google::protobuf::Message& message, EntityPtr& player)
{
	Send2ControllerPlayer(message, (entt::entity)player);
}

void Send2Controller(const google::protobuf::Message& messag, uint32_t controller_node_id)
{
	auto controller_it = g_controller_nodes->find(controller_node_id);
	if (controller_it == g_controller_nodes->end())
	{
		LOG_ERROR << "Send2ControllerPlayer controller not found" << controller_node_id;
		return;
	}
	controller_it->second->session_->Send(messag);
}

void Send2Gate(const google::protobuf::Message& messag, uint32_t gate_node_id)
{
	auto gate_it = g_gate_nodes->find(gate_node_id);
	if (gate_it == g_gate_nodes->end())
	{
		LOG_ERROR << "Send2Gate gate not found" << gate_node_id;
		return;
	}
	auto& gate_node = registry.get<GateNodePtr>(gate_it->second);
	gate_node->session_.Send(messag);
}

