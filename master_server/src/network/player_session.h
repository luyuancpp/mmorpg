#ifndef MASTER_SERVER_SRC_NETWORK_PLAYER_SESSION_H_
#define MASTER_SERVER_SRC_NETWORK_PLAYER_SESSION_H_

#include "gate_node.h"
#include "src/game_logic/comp/gs_scene_comp.hpp"
#include "src/game_logic/comp/player_comp.hpp"

namespace master
{
	class PlayerSession
	{
	public:

		uint32_t gs_node_id()const
		{
			auto gs = gs_.lock();
			if (nullptr == gs)
			{
				return common::kInvalidU32Id;
			}
			return gs->node_id();
		}

		void Send2Gate(::google::protobuf::Message& message)
		{
			auto gate = gate_.lock();
			if (nullptr == gate)
			{
				return;
			}
			gate->session_.Send(message);
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


		common::GateConnId gate_conn_id_;
		GateNodeWPtr gate_;
		common::GSDataWeakPtr gs_;
	};
}//namespace master

#endif//MASTER_SERVER_SRC_NETWORK_PLAYER_SESSION_H_
