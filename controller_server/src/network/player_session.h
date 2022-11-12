#pragma once

#include "gate_node.h"
#include "src/game_logic/comp/scene_comp.h"
#include "src/game_logic/game_registry.h"
#include "src/network/gs_node.h"
#include "src/network/message_system.h"
#include "src/network/session.h"

#include "component_proto/player_network_comp.pb.h"

uint32_t controller_node_id();

class PlayerSession
{
public:

	uint32_t gs_node_id()const
	{
		if (nullptr == gs_)
		{
			return kInvalidU32Id;
		}
		return gs_->node_id();
	}

	inline decltype(auto) gate_node_id()const
	{
		return node_id(gate_session_.session_id());
	}

	inline decltype(auto) session_id()const { return gate_session_.session_id(); }
	inline void set_gs(GsNodePtr& gs) { gs_ = gs; }
	inline GsNodePtr& gs(){ return gs_; }

	inline void Send(const ::google::protobuf::Message& message)
	{
		GateNodePtr gate = gate_.lock();
		if (nullptr == gate)
		{
			return;
		}
		Send2Player(message, gate, gate_session_.session_id());
	}

	void Send2Gs(::google::protobuf::Message& message)
	{
		//gs->.Send(message);
	}
	GateSession gate_session_;
	GateNodeWPtr gate_;
private:
	GsNodePtr gs_;
};

