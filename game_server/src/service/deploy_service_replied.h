#pragma once

#include "muduo/net/TcpConnection.h"

#include "src/pb/pbc/common_proto/deploy_service.pb.h"

using namespace muduo;
using namespace muduo::net;

using ServerInfoResponsePtr = std::shared_ptr<ServerInfoResponse>;
void OnServerInfoReplied(const TcpConnectionPtr& conn, const ServerInfoResponsePtr& replied, Timestamp timestamp);

using StartGSResponsePtr = std::shared_ptr<StartGSResponse>;
void StartGsDeployReplied(const TcpConnectionPtr& conn, const StartGSResponsePtr& replied, Timestamp timestamp);

using LobbyInfoResponsePtr = std::shared_ptr<LobbyInfoResponse>;
void OnAcquireLobbyInfoReplied(const TcpConnectionPtr& conn, const LobbyInfoResponsePtr& replied, Timestamp timestamp);