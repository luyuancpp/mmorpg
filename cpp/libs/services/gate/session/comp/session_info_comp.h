#pragma once

#include <unordered_map>

#include "muduo/net/TcpConnection.h"
#include "engine/core/type_define/type_define.h"
#include "message_limiter/message_limiter.h"
#include "node/system/node/node_util.h"

struct SessionInfo {
	using NodeMap = std::unordered_map<uint32_t, NodeId>;

	SessionInfo() = default;

	void SetNodeId(uint32_t nodeType, NodeId nodeId) {
		nodeIds[nodeType] = nodeId;
	}

	NodeId GetNodeId(uint32_t nodeType) const {
		auto it = nodeIds.find(nodeType);
		if (it != nodeIds.end()) {
			return it->second;
		}
		return kInvalidNodeId;
	}

	bool HasNodeId(uint32_t nodeType) const {
		auto it = nodeIds.find(nodeType);
		if (it == nodeIds.end())
		{
			return false;
		}
		return it->second != kInvalidNodeId;
	}

	Guid playerId{ kInvalidGuid };
	muduo::net::TcpConnectionPtr conn;
	MessageLimiter messageLimiter;
	uint64_t sessionVersion{ kInvalidGuid };
	uint32_t pendingEnterGsType{ 0 }; // Pending login type to forward to Scene once scene node is assigned
private:
	NodeMap nodeIds; // Sparse map, only stores assigned nodes
};



