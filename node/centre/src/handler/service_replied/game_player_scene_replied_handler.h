#pragma once
#include "logic/server_player/game_player_scene.pb.h"
#include "muduo/net/TcpConnection.h"

using namespace muduo;
using namespace muduo::net;

void OnGamePlayerSceneServiceEnterSceneRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<google::protobuf::Empty>& replied, Timestamp timestamp);

void OnGamePlayerSceneServiceLeaveSceneRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<google::protobuf::Empty>& replied, Timestamp timestamp);

void OnGamePlayerSceneServiceEnterSceneS2CRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<EnterScenerS2CResponse>& replied, Timestamp timestamp);

