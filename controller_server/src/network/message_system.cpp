#include "message_system.h"

#include "muduo/base/Logging.h"

#include "src/common_type/common_type.h"
#include "src/game_logic/comp/scene_comp.h"
#include "src/comp/player_list.h"
#include "src/network/gs_node.h"
#include "src/network/player_session.h"
#include "src/network/gate_node.h"
#include "src/network/server_component.h"
#include "src/pb/pbc/msgmap.h"

#include "gate_service.pb.h"
#include "game_service.pb.h"

void Send2Gs(const google::protobuf::Message& message, uint32_t node_id)
{
	auto it = g_gs_nodes.find(node_id);
	if (it == g_gs_nodes.end())
	{
		LOG_INFO << "gs not found ->" << node_id;
		return;
	}
	auto node =  registry.try_get<GsNodePtr>(it->second);
	if (nullptr == node)
	{
		LOG_INFO << "gs not found ->" << node_id;
		return;
	}
	(*node)->session_.Send(message);
}

void Send2GsPlayer(const google::protobuf::Message& message, entt::entity player)
{
	if (!registry.valid(player))
	{
		return;
	}
	auto try_player_session = registry.try_get<PlayerSession>(player);
	if (nullptr == try_player_session)
	{
		return;
	}
	auto gs = try_player_session->gs();
	if (nullptr == gs)
	{
		LOG_INFO << "gs not found ";
		return;
	}
	auto msg_it = g_msgid.find(message.GetDescriptor()->full_name());
	if (msg_it == g_msgid.end())
	{
		LOG_ERROR << "message id not found " << message.GetDescriptor()->full_name();
		return;
	}
	gsservice::NodeServiceMessageRequest msg;
	msg.mutable_msg()->set_msg_id(msg_it->second);
	msg.mutable_msg()->set_body(message.SerializeAsString());
	msg.mutable_ex()->set_player_id(registry.get<Guid>(player));
	gs->session_.Send(msg);
}

void Send2GsPlayer(const google::protobuf::Message& message, EntityPtr& player)
{
	Send2GsPlayer(message, (entt::entity)player);
}

void Send2GsPlayer(const google::protobuf::Message& message, Guid player_id)
{
    Send2GsPlayer(message, g_player_list->GetPlayer(player_id));
}

void Send2PlayerViaGs(const google::protobuf::Message& message, Guid player_id)
{
	Send2PlayerViaGs(message, g_player_list->GetPlayer(player_id));
}

void Send2PlayerViaGs(const google::protobuf::Message& message, EntityPtr& player)
{
	Send2PlayerViaGs(message, (entt::entity)player);
}

void Send2PlayerViaGs(const google::protobuf::Message& message, entt::entity player)
{
    if (!registry.valid(player))
    {
        return;
    }
    auto player_session = registry.get<PlayerSession>(player);
    auto gs = player_session.gs();
    if (nullptr == gs)
    {
        LOG_INFO << "gs not found ";
        return;
    }
    auto msg_it = g_msgid.find(message.GetDescriptor()->full_name());
    if (msg_it == g_msgid.end())
    {
        LOG_ERROR << "message id not found " << message.GetDescriptor()->full_name();
        return;
    }
    gsservice::ControllerSend2PlayerViaGsRequest msg;
    msg.mutable_msg()->set_msg_id(msg_it->second);
    msg.mutable_msg()->set_body(message.SerializeAsString());
    msg.mutable_ex()->set_player_id(registry.get<Guid>(player));
	gs->session_.Send(msg);
}

void Send2Player(const google::protobuf::Message& message, entt::entity player)
{
	if (!registry.valid(player))
	{
		return;
	}
	auto player_session = registry.get<PlayerSession>(player);
	auto gate = player_session.gate_.lock();
	if (nullptr == gate)
	{
		return;
	}
	Send2Player(message, gate, player_session.gate_session_.session_id());
}

void Send2Player(const google::protobuf::Message& message, GateNodePtr& gate, uint64_t session_id)
{
    auto message_it = g_msgid.find(message.GetDescriptor()->full_name());
    if (message_it == g_msgid.end())
    {
        LOG_ERROR << "message id not found " << message.GetDescriptor()->full_name();
        return;
    }
    gateservice::PlayerMessageRequest msg_wrapper;
    msg_wrapper.mutable_ex()->set_session_id(session_id);
    msg_wrapper.mutable_msg()->set_body(message.SerializeAsString());
    msg_wrapper.mutable_msg()->set_msg_id(message_it->second);
    gate->session_.Send(msg_wrapper);
}

void Send2Player(const google::protobuf::Message& message, Guid player_id)
{
	auto player = g_player_list->GetPlayer(player_id);
	Send2Player(message, player);
}

void Send2Gate(const google::protobuf::Message& message, uint32_t gate_id)
{
	auto gate_it = g_gate_nodes.find(gate_id);
	if (gate_it == g_gate_nodes.end())
	{
		return;
	}
	auto gate = registry.try_get<GateNodePtr>(gate_it->second);
	(*gate)->session_.Send(message);
}
