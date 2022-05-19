#pragma once

#include "gate_node.h"
#include "src/game_logic/comp/scene_comp.h"
#include "src/game_logic/game_registry.h"
#include "src/game_logic/comp/player_comp.h"
#include "src/network/message_system.h"

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
		auto gate = gate_.lock();
		if (nullptr == gate)
		{
			return kInvalidU32Id;
		}
		return gate->node_id();
	}

	inline void Send(::google::protobuf::Message& message)
	{
		Send2Player(message, reg.get<Guid>(player_));
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

	entt::entity player_{ entt::null};
	GateConnId gate_conn_id_;
	GateNodeWPtr gate_;
	GsDataWeakPtr gs_;
};

