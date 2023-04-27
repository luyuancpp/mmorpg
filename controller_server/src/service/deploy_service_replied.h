#pragma once

#include "muduo/net/TcpConnection.h"

#include "src/pb/pbc/common_proto/deploy_service.pb.h"

using namespace muduo;
using namespace muduo::net;

using ServerInfoResponsePtr = std::shared_ptr<ServerInfoResponse>;
void OnServerInfoReplied(const TcpConnectionPtr& conn, const ServerInfoResponsePtr& replied, Timestamp timestamp);

using SceneSqueueResponesePtr = std::shared_ptr<SceneSqueueResponese>;
void SceneSqueueNodeIdReplied(const TcpConnectionPtr& conn, const SceneSqueueResponesePtr& replied, Timestamp timestamp);