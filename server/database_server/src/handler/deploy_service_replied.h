#pragma once

#include "muduo/net/TcpConnection.h"

#include "common_proto/deploy_service.pb.h"

using namespace muduo;
using namespace muduo::net;

using GruoupLoginNodeResponsePtr = std::shared_ptr<GruoupLoginNodeResponse>;
void OnLoginNodeInfoReplied(const TcpConnectionPtr& conn, const GruoupLoginNodeResponsePtr& replied, Timestamp timestamp);

using ServerInfoResponsePtr = std::shared_ptr<ServerInfoResponse>;
void OnServerInfoReplied(const TcpConnectionPtr& conn, const ServerInfoResponsePtr& replied, Timestamp timestamp);