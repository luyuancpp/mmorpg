#ifndef GAME_SERVER_MODULE_NETWORK_GATE_NODE_H_
#define GAME_SERVER_MODULE_NETWORK_GATE_NODE_H_

#include "muduo/net/TcpConnection.h"

#include "src/game_logic/entity/entity.h"
#include "src/server_common/node_info.h"
#include "src/server_common/server_component.h"

#include "common.pb.h"

namespace game
{
	struct GateNode
	{
		GateNode(const muduo::net::TcpConnectionPtr& conn)
			: session_(conn) {}

		inline uint32_t node_id() const { return node_info_.node_id(); }

		NodeInfo node_info_;
		common::RpcServerConnection session_;
	};
	using GateNodePtr = std::shared_ptr<GateNode>;
	using GateNodeWPtr = std::weak_ptr<GateNode>;
	struct GateNodes : public std::unordered_map<uint32_t, entt::entity> {};
}//namespace game

#endif//GAME_SERVER_MODULE_NETWORK_GATE_NODE_H_
