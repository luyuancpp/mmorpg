
#include "centre_service_response_handler.h"

#include "rpc/service_metadata/centre_service_service_metadata.h"
#include "network/codec/message_response_dispatcher.h"

extern MessageResponseDispatcher gRpcResponseDispatcher;


///<<< BEGIN WRITING YOUR CODE
#include "scene/scene/system/game_node_scene_system.h"
#include "proto/common/node.pb.h"
#include "proto/logic/event/server_event.pb.h"

#include "scene_node.h"
///<<< END WRITING YOUR CODE



void InitCentreReply()
{
    gRpcResponseDispatcher.registerMessageCallback<::Empty>(CentreGatePlayerServiceMessageId,
        std::bind(&OnCentreGatePlayerServiceReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::Empty>(CentreGateSessionDisconnectMessageId,
        std::bind(&OnCentreGateSessionDisconnectReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::CentreLoginResponse>(CentreLoginNodeAccountLoginMessageId,
        std::bind(&OnCentreLoginNodeAccountLoginReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::Empty>(CentreLoginNodeEnterGameMessageId,
        std::bind(&OnCentreLoginNodeEnterGameReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::Empty>(CentreLoginNodeLeaveGameMessageId,
        std::bind(&OnCentreLoginNodeLeaveGameReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::Empty>(CentreLoginNodeSessionDisconnectMessageId,
        std::bind(&OnCentreLoginNodeSessionDisconnectReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::NodeRouteMessageResponse>(CentrePlayerServiceMessageId,
        std::bind(&OnCentrePlayerServiceReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::Empty>(CentreEnterGsSucceedMessageId,
        std::bind(&OnCentreEnterGsSucceedReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::RouteMessageResponse>(CentreRouteNodeStringMsgMessageId,
        std::bind(&OnCentreRouteNodeStringMsgReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::RoutePlayerMessageResponse>(CentreRoutePlayerStringMsgMessageId,
        std::bind(&OnCentreRoutePlayerStringMsgReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::Empty>(CentreInitSceneNodeMessageId,
        std::bind(&OnCentreInitSceneNodeReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gRpcResponseDispatcher.registerMessageCallback<::NodeHandshakeResponse>(CentreNodeHandshakeMessageId,
        std::bind(&OnCentreNodeHandshakeReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnCentreGatePlayerServiceReply(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnCentreGateSessionDisconnectReply(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnCentreLoginNodeAccountLoginReply(const TcpConnectionPtr& conn, const std::shared_ptr<::CentreLoginResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnCentreLoginNodeEnterGameReply(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnCentreLoginNodeLeaveGameReply(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnCentreLoginNodeSessionDisconnectReply(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnCentrePlayerServiceReply(const TcpConnectionPtr& conn, const std::shared_ptr<::NodeRouteMessageResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnCentreEnterGsSucceedReply(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnCentreRouteNodeStringMsgReply(const TcpConnectionPtr& conn, const std::shared_ptr<::RouteMessageResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnCentreRoutePlayerStringMsgReply(const TcpConnectionPtr& conn, const std::shared_ptr<::RoutePlayerMessageResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnCentreInitSceneNodeReply(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnCentreNodeHandshakeReply(const TcpConnectionPtr& conn, const std::shared_ptr<::NodeHandshakeResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
	gNode->GetNodeRegistrationManager().OnHandshakeReplied(*replied);
///<<< END WRITING YOUR CODE
}
