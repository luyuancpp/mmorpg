#pragma once

#include "muduo/net/TcpConnection.h"

#include "logic_proto/lobby_scene.pb.h"

using namespace muduo;
using namespace muduo::net;

using StartCrossGsResponsePtr = std::shared_ptr<StartCrossGsResponse>;
void OnStartCrossGsReplied(const TcpConnectionPtr& conn, const StartCrossGsResponsePtr& replied, Timestamp timestamp);
