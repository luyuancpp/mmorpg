#pragma once
#include "muduo/net/TcpConnection.h"
#include "proto/turnbased/turnbased_dungeon.pb.h"

void OnTurnBasedDungeonEnterDungeonReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::EnterDungeonResponse>& replied, muduo::Timestamp timestamp);
void OnTurnBasedDungeonMoveInDungeonReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::MoveInDungeonResponse>& replied, muduo::Timestamp timestamp);
void OnTurnBasedDungeonNextFloorReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::NextFloorResponse>& replied, muduo::Timestamp timestamp);
void OnTurnBasedDungeonQueryDungeonReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::QueryDungeonResponse>& replied, muduo::Timestamp timestamp);
void OnTurnBasedDungeonLeaveDungeonReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::LeaveDungeonResponse>& replied, muduo::Timestamp timestamp);
void OnTurnBasedDungeonNodeHandshakeReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::NodeHandshakeResponse>& replied, muduo::Timestamp timestamp);
