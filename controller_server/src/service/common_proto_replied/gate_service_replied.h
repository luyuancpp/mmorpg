#pragma once

#include <memory>

#include "muduo/net/TcpConnection.h"

#include "src/pb/pbc/common_proto/gate_service.pb.h"

using namespace muduo;
using namespace muduo::net;

using PlayerEnterGsResponesePtr = std::shared_ptr<gateservice::PlayerEnterGsResponese>;
void OnGateUpdatePlayerGsReplied(const TcpConnectionPtr& conn, const PlayerEnterGsResponesePtr& replied, Timestamp timestamp);

