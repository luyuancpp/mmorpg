#pragma once
#include <cstdint>

#include "muduo/net/TcpConnection.h"

#include "type_define/type_define.h"

NodeId GetGateNodeId(uint64_t session_id);

uint64_t tcp_session_id(const muduo::net::TcpConnectionPtr& conn);