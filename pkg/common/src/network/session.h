#pragma once
#include <cstdint>

#include "muduo/net/TcpConnection.h"

#include "src/type_define/type_define.h"

NodeId get_gate_node_id(uint64_t session_id);

uint64_t tcp_session_id(const muduo::net::TcpConnectionPtr& conn);