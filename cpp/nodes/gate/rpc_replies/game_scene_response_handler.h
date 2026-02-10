#pragma once
#include "muduo/net/TcpConnection.h"
#include "proto/room/game_scene.pb.h"

using namespace muduo;
using namespace muduo::net;
void OnSceneSceneTestReply(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp);
