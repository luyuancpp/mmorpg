#pragma once
#include "logic/server/centre_scene.pb.h"
#include "muduo/net/TcpConnection.h"
using namespace muduo;
using namespace muduo::net;

void OnCentreSceneServiceRegisterSceneRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<RegisterSceneResponse>& replied, Timestamp timestamp);

void OnCentreSceneServiceUnRegisterSceneRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<Empty>& replied, Timestamp timestamp);

