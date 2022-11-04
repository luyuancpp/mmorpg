#pragma once

#include "muduo/net/TcpConnection.h"

#include "src/common_type/common_type.h"

struct GateClient
{
    bool ValidLogin()const { return login_node_id_ != UINT32_MAX; }
	uint32_t gs_node_id_{ UINT32_MAX };
	uint32_t login_node_id_{ UINT32_MAX };
	muduo::net::TcpConnectionPtr conn_;
};

using ClientSessions = std::unordered_map<uint64_t, GateClient>;

extern ClientSessions* g_client_sessions_;

