#pragma once
#include "muduo/net/TcpConnection.h"
#include "proto/turnbased/turnbased_battle.pb.h"

void OnTurnBasedBattleStartBattleReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::StartBattleResponse>& replied, muduo::Timestamp timestamp);
void OnTurnBasedBattleExecuteActionReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::ExecuteActionResponse>& replied, muduo::Timestamp timestamp);
void OnTurnBasedBattleQueryBattleReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::QueryBattleResponse>& replied, muduo::Timestamp timestamp);
void OnTurnBasedBattleNodeHandshakeReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::NodeHandshakeResponse>& replied, muduo::Timestamp timestamp);
