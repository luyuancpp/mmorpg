#pragma once
#include "logic/server_player/game_player.pb.h"
#include "muduo/net/TcpConnection.h"

using namespace muduo;
using namespace muduo::net;

void OnGamePlayerServiceCentre2GsLoginRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<google::protobuf::Empty>& replied, Timestamp timestamp);

