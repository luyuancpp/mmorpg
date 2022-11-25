#pragma once

#include "muduo/net/TcpConnection.h"

#include "src/pb/pbc/common_proto/game_service.pb.h"

using namespace muduo;
using namespace muduo::net;

using RpcClientResponsePtr = std::shared_ptr<gsservice::RpcClientResponse>;
void OnGsPlayerServiceReplied(const TcpConnectionPtr& conn, const RpcClientResponsePtr& replied, Timestamp timestamp);