
#include "turnbased_battle_response_handler.h"

#include "rpc/service_metadata/turnbased_battle_service_metadata.h"
#include "network/codec/message_response_dispatcher.h"

extern MessageResponseDispatcher gRpcResponseDispatcher;

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

void InitTurnBasedBattleReply()
{
    gRpcResponseDispatcher.registerMessageCallback<::StartBattleResponse>(TurnBasedBattleStartBattleMessageId,
        std::bind(&OnTurnBasedBattleStartBattleReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::ExecuteActionResponse>(TurnBasedBattleExecuteActionMessageId,
        std::bind(&OnTurnBasedBattleExecuteActionReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::QueryBattleResponse>(TurnBasedBattleQueryBattleMessageId,
        std::bind(&OnTurnBasedBattleQueryBattleReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::NodeHandshakeResponse>(TurnBasedBattleNodeHandshakeMessageId,
        std::bind(&OnTurnBasedBattleNodeHandshakeReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnTurnBasedBattleStartBattleReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::StartBattleResponse>& replied, muduo::Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE}

void OnTurnBasedBattleExecuteActionReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::ExecuteActionResponse>& replied, muduo::Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE}

void OnTurnBasedBattleQueryBattleReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::QueryBattleResponse>& replied, muduo::Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE}

void OnTurnBasedBattleNodeHandshakeReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::NodeHandshakeResponse>& replied, muduo::Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE}
