
#include "slg_map_response_handler.h"

#include "rpc/service_metadata/slg_map_service_metadata.h"
#include "network/codec/message_response_dispatcher.h"

extern MessageResponseDispatcher gRpcResponseDispatcher;

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

void InitSlgMapReply()
{
    gRpcResponseDispatcher.registerMessageCallback<::StartMarchResponse>(SlgMapStartMarchMessageId,
        std::bind(&OnSlgMapStartMarchReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::Empty>(SlgMapCancelMarchMessageId,
        std::bind(&OnSlgMapCancelMarchReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::ViewportResponse>(SlgMapQueryViewportMessageId,
        std::bind(&OnSlgMapQueryViewportReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::BuildResponse>(SlgMapBuildMessageId,
        std::bind(&OnSlgMapBuildReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::UpgradeBuildingResponse>(SlgMapUpgradeBuildingMessageId,
        std::bind(&OnSlgMapUpgradeBuildingReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::NodeHandshakeResponse>(SlgMapNodeHandshakeMessageId,
        std::bind(&OnSlgMapNodeHandshakeReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnSlgMapStartMarchReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::StartMarchResponse>& replied, muduo::Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE}
}

void OnSlgMapCancelMarchReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, muduo::Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE}
}

void OnSlgMapQueryViewportReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::ViewportResponse>& replied, muduo::Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE}
}

void OnSlgMapBuildReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::BuildResponse>& replied, muduo::Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE}
}

void OnSlgMapUpgradeBuildingReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::UpgradeBuildingResponse>& replied, muduo::Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE}
}

void OnSlgMapNodeHandshakeReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::NodeHandshakeResponse>& replied, muduo::Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE}
}
