#pragma once
#include "muduo/net/TcpConnection.h"
#include "proto/slg/slg_battle.pb.h"

void OnSlgBattleSimulateBattleReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::SimulateBattleResponse>& replied, muduo::Timestamp timestamp);
void OnSlgBattleQueryBattleReportReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::QueryBattleReportResponse>& replied, muduo::Timestamp timestamp);
