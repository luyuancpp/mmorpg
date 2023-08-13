#pragma once

#include "src/network/gate_node.h"
#include "src/network/gs_node.h"
#include "src/network/message_system.h"
#include "src/network/session.h"
#include "src/thread_local/controller_thread_local_storage.h"

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

    inline void set_session_id(uint64_t session_id)
    {
		return gate_session_.set_session_id(session_id);
    }

	inline decltype(auto) session_id()const { return gate_session_.session_id(); }
	inline void set_gs(GsNodePtr& gs) { gs_ = gs; }
	inline GsNodePtr& gs(){ return gs_; }

	inline void Send(uint32_t message_id, const ::google::protobuf::Message& message)
	{
		const auto gate_it = controller_tls.gate_nodes().find(gate_node_id());
		if (gate_it == controller_tls.gate_nodes().end())
		{
			return;
		}
		Send2Player(message_id, message, gate_it->second, gate_session_.session_id());
	}
	
	GateSession gate_session_;
private:
	GsNodePtr gs_;
};

