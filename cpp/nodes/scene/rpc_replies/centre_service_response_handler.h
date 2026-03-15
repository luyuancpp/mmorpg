#pragma once
#include "muduo/net/TcpConnection.h"
#include "proto/centre/centre_service.pb.h"

using namespace muduo;
using namespace muduo::net;
void OnCentreNodeHandshakeReply(const TcpConnectionPtr& conn, const std::shared_ptr<::NodeHandshakeResponse>& replied, Timestamp timestamp);
