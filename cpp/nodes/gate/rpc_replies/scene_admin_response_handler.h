#pragma once
#include "muduo/net/TcpConnection.h"
#include "proto/scene/scene_admin.pb.h"

void OnSceneSceneTestReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, muduo::Timestamp timestamp);
void OnSceneSceneGmGracefulShutdownReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::GmGracefulShutdownResponse>& replied, muduo::Timestamp timestamp);
