
#include "turnbased_dungeon_response_handler.h"

#include "rpc/service_metadata/turnbased_dungeon_service_metadata.h"
#include "network/codec/message_response_dispatcher.h"

extern MessageResponseDispatcher gRpcResponseDispatcher;

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

void InitTurnBasedDungeonReply()
{
    gRpcResponseDispatcher.registerMessageCallback<::EnterDungeonResponse>(TurnBasedDungeonEnterDungeonMessageId,
        std::bind(&OnTurnBasedDungeonEnterDungeonReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::MoveInDungeonResponse>(TurnBasedDungeonMoveInDungeonMessageId,
        std::bind(&OnTurnBasedDungeonMoveInDungeonReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::NextFloorResponse>(TurnBasedDungeonNextFloorMessageId,
        std::bind(&OnTurnBasedDungeonNextFloorReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::QueryDungeonResponse>(TurnBasedDungeonQueryDungeonMessageId,
        std::bind(&OnTurnBasedDungeonQueryDungeonReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::LeaveDungeonResponse>(TurnBasedDungeonLeaveDungeonMessageId,
        std::bind(&OnTurnBasedDungeonLeaveDungeonReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::NodeHandshakeResponse>(TurnBasedDungeonNodeHandshakeMessageId,
        std::bind(&OnTurnBasedDungeonNodeHandshakeReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnTurnBasedDungeonEnterDungeonReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::EnterDungeonResponse>& replied, muduo::Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE}

void OnTurnBasedDungeonMoveInDungeonReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::MoveInDungeonResponse>& replied, muduo::Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE}

void OnTurnBasedDungeonNextFloorReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::NextFloorResponse>& replied, muduo::Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE}

void OnTurnBasedDungeonQueryDungeonReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::QueryDungeonResponse>& replied, muduo::Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE}

void OnTurnBasedDungeonLeaveDungeonReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::LeaveDungeonResponse>& replied, muduo::Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE}

void OnTurnBasedDungeonNodeHandshakeReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::NodeHandshakeResponse>& replied, muduo::Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE}
