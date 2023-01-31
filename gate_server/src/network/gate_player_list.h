#pragma once

#include "muduo/net/TcpConnection.h"

#include "src/common_type/common_type.h"

struct Session
{
	static const uint32_t kInvalidNodeId{ UINT32_MAX };
    bool ValidLogin()const { return login_node_id_ != kInvalidNodeId; }
	uint32_t gs_node_id_{ kInvalidNodeId };
	uint32_t login_node_id_{ kInvalidNodeId };
	muduo::net::TcpConnectionPtr conn_;
};

using ClientSessions = std::unordered_map<uint64_t, Session>;

