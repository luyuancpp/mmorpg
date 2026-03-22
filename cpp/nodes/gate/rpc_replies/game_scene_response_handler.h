#pragma once
#include "muduo/net/TcpConnection.h"
#include "proto/scene/game_scene.pb.h"

void OnSceneSceneTestReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, muduo::Timestamp timestamp);
