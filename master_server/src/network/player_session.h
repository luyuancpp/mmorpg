#pragma once

#include "gate_node.h"
#include "src/game_logic/comp/scene_comp.h"
#include "src/game_logic/game_registry.h"
#include "src/game_logic/comp/player_comp.h"
#include "src/network/message_system.h"
#include "src/network/session.h"

class PlayerSession
{
public:

	uint32_t gs_node_id()const
	{
		auto gs = gs_.lock();
		if (nullptr == gs)
		{
			return kInvalidU32Id;
		}
		return gs->node_id();
	}

	uint32_t gate_node_id()const
	{
		return node_id(gate_conn_id_.session_id_);
	}

	inline void Send(const ::google::protobuf::Message& message)
	{
		GateNodePtr gate = gate_.lock();
		if (nullptr == gate)
		{
			return;
		}
		Send2Player(message, gate, gate_conn_id_.session_id_);
	}

	void Send2Gs(::google::protobuf::Message& message)
	{
		auto gs = gs_.lock();
		if (nullptr == gs)
		{
			return;
		}
		//gs->.Send(message);
	}
	GateSession gate_conn_id_;
	GateNodeWPtr gate_;
	GsDataWeakPtr gs_;
};

