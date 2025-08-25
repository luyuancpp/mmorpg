﻿#pragma once

#include "message_limiter/message_limiter.h"
#include "muduo/net/TcpConnection.h"
#include "base/common/type_define/type_define.h"
#include "proto/common/node.pb.h"
#include <muduo/base/Logging.h>

#include <unordered_map>

struct Session {
	using NodeMap = std::unordered_map<uint32_t, NodeId>;

	Session() = default;

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

	Guid playerGuild{ kInvalidGuid };
	muduo::net::TcpConnectionPtr conn;
	MessageLimiter messageLimiter;

private:
	NodeMap nodeIds; // 稀疏结构，只保存设置过的 node
};



