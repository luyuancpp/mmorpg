#pragma once

#include <memory>

#include "muduo/net/TcpConnection.h"

#include "src/pb/pbc/common_proto/common.pb.h"

using namespace muduo;
using namespace muduo::net;

using NodeServiceMessageResponsePtr = std::shared_ptr<NodeServiceMessageResponse>;
void OnGsCallPlayerReplied(const TcpConnectionPtr& conn, const NodeServiceMessageResponsePtr& replied, Timestamp timestamp);

