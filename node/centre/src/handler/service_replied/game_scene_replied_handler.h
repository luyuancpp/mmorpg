#pragma once
#include "proto/logic/server/game_scene.pb.h"
#include "muduo/net/TcpConnection.h"

using namespace muduo;
using namespace muduo::net;

void OnGameSceneServiceTestRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp);

