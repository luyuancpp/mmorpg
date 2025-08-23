#pragma once
#include "muduo/net/TcpConnection.h"
#include "proto/centre/centre_scene.pb.h"

using namespace muduo;
using namespace muduo::net;
void OnCentreSceneRegisterSceneRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::RegisterSceneResponse>& replied, Timestamp timestamp);
void OnCentreSceneUnRegisterSceneRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp);
