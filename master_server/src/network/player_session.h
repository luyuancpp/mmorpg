#ifndef MASTER_SERVER_SRC_NETWORK_PLAYER_SESSION_H_
#define MASTER_SERVER_SRC_NETWORK_PLAYER_SESSION_H_

#include "gate_node.h"
#include "src/game_logic/comp/gs_scene_comp.h"
#include "src/game_logic/game_registry.h"
#include "src/game_logic/comp/player_comp.h"
#include "src/network/message_sys.h"

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

		uint32_t gate_node_id()const
		{
			auto gate = gate_.lock();
			if (nullptr == gate)
			{
				return common::kInvalidU32Id;
			}
			return gate->node_id();
		}

		inline void Send(::google::protobuf::Message& message)
		{
			Send2Player(message, common::reg.get<common::Guid>(player_));
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
		common::GateConnId gate_conn_id_;
		GateNodeWPtr gate_;
		common::GsDataWeakPtr gs_;
	};
#endif//MASTER_SERVER_SRC_NETWORK_PLAYER_SESSION_H_
