#include "message_sys.h"

#include "muduo/base/Logging.h"

#include "src/common_type/common_type.h"
#include "src/game_logic/comp/gs_scene_comp.hpp"
#include "src/master_player/ms_player_list.h"
#include "src/network/gs_node.h"
#include "src/network/player_session.h"
#include "src/network/gate_node.h"
#include "src/server_common/server_component.h"
#include "src/pb/pbc/msgmap.h"

#include "gw_node.pb.h"
#include "gs_node.pb.h"

using namespace common;


void Send2Gs(const google::protobuf::Message& message, uint32_t node_id)
{
		auto it = g_gs_nodes.find(node_id);
		if (it == g_gs_nodes.end())
		{
			LOG_INFO << "gs not found ->" << node_id;
			return;
		}
		auto node =  reg.try_get<GsNodePtr>(it->second);
		if (nullptr == node)
		{
			LOG_INFO << "gs not found ->" << node_id;
			return;
		}
		(*node)->session_.Send(message);
}

void Send2GsPlayer(const google::protobuf::Message& message, entt::entity player)
{
	if (!reg.valid(player))
	{
		return;
	}
	auto player_session = reg.get<PlayerSession>(player);
	auto gs = player_session.gs_.lock();
	if (nullptr == gs)
	{
		LOG_INFO << "gs not found ";
		return;
	}
	auto message_it = g_msgid.find(message.GetDescriptor()->full_name());
	if (message_it == g_msgid.end())
	{
		LOG_ERROR << "message id not found " << message.GetDescriptor()->full_name();
		return;
	}
	gsservice::MsPlayerMessageRequest ms2gs_messag;
	ms2gs_messag.mutable_msg()->set_msg_id(message_it->second);
	ms2gs_messag.mutable_msg()->set_body(message.SerializeAsString());
	ms2gs_messag.mutable_ex()->set_player_id(reg.get<common::Guid>(player));
	auto& gs_session = reg.get<RpcServerConnection>(gs->server_entity());
	gs_session.Send(ms2gs_messag);
}

void Send2GsPlayer(const google::protobuf::Message& message, common::EntityPtr& entity)
{
	Send2GsPlayer(message, entity.entity());
}

void Send2GsPlayer(const google::protobuf::Message& message, common::Guid player_id)
{
	auto player = PlayerList::GetSingleton().GetPlayer(player_id);
	Send2GsPlayer(message, player);
}

void Send2Player(const google::protobuf::Message& message, entt::entity player)
{
	if (!reg.valid(player))
	{
		return;
	}
	auto player_session = reg.get<PlayerSession>(player);
	auto gate = player_session.gate_.lock();
	if (nullptr == gate)
	{
		return;
	}
	auto message_it = g_msgid.find(message.GetDescriptor()->full_name());
	if (message_it == g_msgid.end())
	{
		LOG_ERROR << "message id not found " << message.GetDescriptor()->full_name();
		return;
	}
	gwservice::PlayerMessageRequest msg_wrapper;
	msg_wrapper.mutable_ex()->set_conn_id(player_session.gate_conn_id_.conn_id_);
	msg_wrapper.mutable_msg()->set_body(message.SerializeAsString());
	msg_wrapper.mutable_msg()->set_msg_id(message_it->second);
	gate->session_.Send(msg_wrapper);
}

void Send2Player(const google::protobuf::Message& message, common::Guid player_id)
{
	auto player = PlayerList::GetSingleton().GetPlayer(player_id);
	Send2Player(message, player);
}

void Send2Gate(const google::protobuf::Message& message, uint32_t gate_id)
{
	auto gate_it = g_gate_nodes.find(gate_id);
	if (gate_it == g_gate_nodes.end())
	{
		return;
	}
	auto gate = reg.try_get<GateNodePtr>(gate_it->second);
	(*gate)->session_.Send(message);
}
