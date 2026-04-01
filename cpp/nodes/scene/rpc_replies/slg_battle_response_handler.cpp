
#include "slg_battle_response_handler.h"

#include "rpc/service_metadata/slg_battle_service_metadata.h"
#include "network/codec/message_response_dispatcher.h"

extern MessageResponseDispatcher gRpcResponseDispatcher;

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

void InitSlgBattleReply()
{
    gRpcResponseDispatcher.registerMessageCallback<::SimulateBattleResponse>(SlgBattleSimulateBattleMessageId,
        std::bind(&OnSlgBattleSimulateBattleReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::QueryBattleReportResponse>(SlgBattleQueryBattleReportMessageId,
        std::bind(&OnSlgBattleQueryBattleReportReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnSlgBattleSimulateBattleReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::SimulateBattleResponse>& replied, muduo::Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE}
}

void OnSlgBattleQueryBattleReportReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::QueryBattleReportResponse>& replied, muduo::Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE}
}
