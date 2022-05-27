#ifndef GATEWAY_SERVER_SRC_GATEWAY_PLAYER_GATEWAY_PLAYER_LIST_H_
#define GATEWAY_SERVER_SRC_GATEWAY_PLAYER_GATEWAY_PLAYER_LIST_H_

#include "muduo/net/TcpConnection.h"

#include "src/common_type/common_type.h"

struct GateClient
{
	Guid player_id_{ kInvalidGuid };
	uint32_t gs_node_id_{ UINT32_MAX };
	muduo::net::TcpConnectionPtr conn_;
};

using ClientSessions = std::unordered_map<uint64_t, GateClient>;

extern ClientSessions* g_client_sessions_;


#endif//GATEWAY_SERVER_SRC_GATEWAY_PLAYER_GATEWAY_PLAYER_LIST_H_
