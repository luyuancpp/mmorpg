#pragma once
#include "muduo/net/TcpConnection.h"
#include "proto/service/rpc/centre/centre_scene.pb.h"

using namespace muduo;
using namespace muduo::net;
void OnCentreSceneRegisterSceneReply(const TcpConnectionPtr& conn, const std::shared_ptr<::RegisterSceneResponse>& replied, Timestamp timestamp);
void OnCentreSceneUnRegisterSceneReply(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp);
