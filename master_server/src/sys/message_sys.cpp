#include "message_sys.hpp"

#include "src/common_type/common_type.h"
#include "src/game_logic/comp/gs_scene_comp.hpp"
#include "src/master_player/ms_player_list.h"
#include "src/network/player_session.h"
#include "src/network/gate_node.h"
#include "src/server_common/server_component.h"
#include "src/pb/pbc/msgmap.h"

#include "ms2gw.pb.h"

using namespace common;

namespace master
{
	void Send2Gs(const google::protobuf::Message& message, uint32_t node_id)
{
		auto& gs_nodes = reg.get<GsNodes>(global_entity());
		auto it = gs_nodes.find(node_id);
		if (it == gs_nodes.end())
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

void Send2GsPlayer(const google::protobuf::Message& message, entt::entity player_entity)
{
	auto ptr_gse = reg.try_get<GSEntity>(player_entity);
	if (nullptr == ptr_gse)
	{
		LOG_ERROR << "player send message empty server:" << reg.get<Guid>(player_entity)
			<< "message:" << message.GetTypeName();
		return;
	}
	auto& gs = reg.get<RpcServerConnection>(ptr_gse->server_entity());
	gs.Send(message);
}

void Send2Player(const google::protobuf::Message& message, common::Guid player_id)
{
	auto player = PlayerList::GetSingleton().GetPlayer(player_id);
	if (entt::null == player)
	{
		return;
	}
	auto player_session = reg.get<PlayerSession>(player);
	auto gate = player_session.gate_.lock();
	if (nullptr == gate)
	{
		return;
	}
	auto it = g_msgid.find(message.GetDescriptor()->full_name());
	if (it == g_msgid.end())
	{
		return;
	}	
	ms2gw::Ms2PlayerMessageRequest ms2gw_messag;
	ms2gw_messag.mutable_request_extern()->set_conn_id(player_session.gate_conn_id_.conn_id_);
	ms2gw_messag.mutable_player_message()->set_response(message.SerializeAsString());
	ms2gw_messag.mutable_player_message()->set_msg_id(it->second);
	gate->session_.Send(message);
}

void Send2Gate(const google::protobuf::Message& message, uint32_t gate_id)
{
	auto& gate_nodes = reg.get<GateNodes>(global_entity());
	auto gate_it = gate_nodes.find(gate_id);
	if (gate_it == gate_nodes.end())
	{
		return;
	}
	auto gate = reg.try_get<GateNodePtr>(gate_it->second);
	(*gate)->session_.Send(message);
}

}//namespace master