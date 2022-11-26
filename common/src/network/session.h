#pragma once
#include <cstdint>

#include "muduo/net/TcpConnection.h"

#include "src/common_type/common_type.h"

NodeId node_id(uint64_t sesion_id);

NodeId node_id();

uint64_t tcp_session_id(const muduo::net::TcpConnectionPtr& conn);