
#include "gate_service_response_handler.h"

#include "rpc/service_metadata/gate_service_service_metadata.h"
#include "network/codec/message_response_dispatcher.h"

extern MessageResponseDispatcher gRpcResponseDispatcher;

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

void InitGateReply()
{
    gRpcResponseDispatcher.registerMessageCallback<::RegisterGameNodeSessionResponse>(GatePlayerEnterGameNodeMessageId,
        std::bind(&OnGatePlayerEnterGameNodeReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::Empty>(GateSendMessageToPlayerMessageId,
        std::bind(&OnGateSendMessageToPlayerReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::RouteMessageResponse>(GateRouteNodeMessageMessageId,
        std::bind(&OnGateRouteNodeMessageReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::RoutePlayerMessageResponse>(GateRoutePlayerMessageMessageId,
        std::bind(&OnGateRoutePlayerMessageReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::Empty>(GateBroadcastToPlayersMessageId,
        std::bind(&OnGateBroadcastToPlayersReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::Empty>(GateBroadcastToSceneMessageId,
        std::bind(&OnGateBroadcastToSceneReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::Empty>(GateBroadcastToAllMessageId,
        std::bind(&OnGateBroadcastToAllReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::NodeHandshakeResponse>(GateNodeHandshakeMessageId,
        std::bind(&OnGateNodeHandshakeReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::BindSessionToGateResponse>(GateBindSessionToGateMessageId,
        std::bind(&OnGateBindSessionToGateReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::GmGracefulShutdownResponse>(GateGmGracefulShutdownMessageId,
        std::bind(&OnGateGmGracefulShutdownReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnGatePlayerEnterGameNodeReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::RegisterGameNodeSessionResponse>& replied, muduo::Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGateSendMessageToPlayerReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, muduo::Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGateRouteNodeMessageReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::RouteMessageResponse>& replied, muduo::Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGateRoutePlayerMessageReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::RoutePlayerMessageResponse>& replied, muduo::Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGateBroadcastToPlayersReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, muduo::Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGateBroadcastToSceneReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, muduo::Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE}
}

void OnGateBroadcastToAllReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, muduo::Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE}
}

void OnGateNodeHandshakeReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::NodeHandshakeResponse>& replied, muduo::Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGateBindSessionToGateReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::BindSessionToGateResponse>& replied, muduo::Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnGateGmGracefulShutdownReply(const muduo::net::TcpConnectionPtr& conn, const std::shared_ptr<::GmGracefulShutdownResponse>& replied, muduo::Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE}
}
