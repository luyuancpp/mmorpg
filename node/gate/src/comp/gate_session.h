#pragma once

#include "message_limiter/message_limiter.h"
#include "muduo/net/TcpConnection.h"
#include "type_define/type_define.h"
#include "proto/common/node.pb.h"
#include <muduo/base/Logging.h>

struct Session
{
	[[nodiscard]] bool HasNodeId(uint32_t nodeType) const { return GetNodeId(nodeType) != kInvalidNodeId; }
	using NodeIds = std::array<NodeId, eNodeType_ARRAYSIZE>;

	Session() {
		nodeIds.fill(kInvalidNodeId);
	}

	NodeId GetNodeId(uint32_t nodeType) const
	{
		if (nodeType >= nodeIds.size())
		{
			LOG_ERROR << "Invalid node type: " << nodeType;
			return kInvalidNodeId;
		}
		return nodeIds[nodeType];
	}

	void SetNodeId(uint32_t nodeType, NodeId nodeId)
	{
		if (nodeType >= nodeIds.size())
		{
			LOG_ERROR << "Invalid node type: " << nodeType;
			return;
		}
		nodeIds[nodeType] = nodeId;
	}

	Guid playerGuild{ kInvalidGuid };
	muduo::net::TcpConnectionPtr conn;
	MessageLimiter messageLimiter;
	NodeIds nodeIds;
};


