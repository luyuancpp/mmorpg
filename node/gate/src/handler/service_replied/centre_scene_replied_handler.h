#pragma once
#include "muduo/net/TcpConnection.h"
#include "proto/centre/centre_scene.pb.h"

using namespace muduo;
using namespace muduo::net;
void OnCentreSceneServiceRegisterSceneRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::RegisterSceneResponse>& replied, Timestamp timestamp);
void OnCentreSceneServiceUnRegisterSceneRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp);
