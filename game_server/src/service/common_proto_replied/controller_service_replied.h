#pragma once

#include "muduo/net/TcpConnection.h"

#include "controller_service.pb.h"

using namespace muduo;
using namespace muduo::net;

using StartGsResponsePtr = std::shared_ptr< controllerservice::StartGsResponse>;
void StartGsControllerReplied(const TcpConnectionPtr& conn, const StartGsResponsePtr& replied, Timestamp timestamp);
