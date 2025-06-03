#pragma once
#include "muduo/net/TcpConnection.h"
#include "proto/scene/game_scene.pb.h"

using namespace muduo;
using namespace muduo::net;
void OnGameSceneServiceTestRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp);
