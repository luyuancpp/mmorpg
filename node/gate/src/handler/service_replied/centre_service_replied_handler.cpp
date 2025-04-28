
#include "centre_service_replied_handler.h"
#include "network/codec/dispatcher.h"

extern ProtobufDispatcher gResponseDispatcher;


///<<< BEGIN WRITING YOUR CODE
#include "gate_node.h"
///<<< END WRITING YOUR CODE



void InitCentreServiceGatePlayerServiceRepliedHandler()
{
    gResponseDispatcher.registerMessageCallback<::Empty>(
        std::bind(&OnCentreServiceGatePlayerServiceRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::Empty>(
        std::bind(&OnCentreServiceGateSessionDisconnectRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::LoginResponse>(
        std::bind(&OnCentreServiceLoginNodeAccountLoginRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::Empty>(
        std::bind(&OnCentreServiceLoginNodeEnterGameRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::Empty>(
        std::bind(&OnCentreServiceLoginNodeLeaveGameRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::Empty>(
        std::bind(&OnCentreServiceLoginNodeSessionDisconnectRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::NodeRouteMessageResponse>(
        std::bind(&OnCentreServicePlayerServiceRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::Empty>(
        std::bind(&OnCentreServiceEnterGsSucceedRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::RouteMessageResponse>(
        std::bind(&OnCentreServiceRouteNodeStringMsgRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::RoutePlayerMessageResponse>(
        std::bind(&OnCentreServiceRoutePlayerStringMsgRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::Empty>(
        std::bind(&OnCentreServiceInitSceneNodeRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::RegisterNodeSessionResponse>(
        std::bind(&OnCentreServiceRegisterNodeSessionRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnCentreServiceGatePlayerServiceRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnCentreServiceGateSessionDisconnectRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnCentreServiceLoginNodeAccountLoginRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::LoginResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnCentreServiceLoginNodeEnterGameRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnCentreServiceLoginNodeLeaveGameRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnCentreServiceLoginNodeSessionDisconnectRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnCentreServicePlayerServiceRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::NodeRouteMessageResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnCentreServiceEnterGsSucceedRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnCentreServiceRouteNodeStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::RouteMessageResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnCentreServiceRoutePlayerStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::RoutePlayerMessageResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnCentreServiceInitSceneNodeRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnCentreServiceRegisterNodeSessionRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::RegisterNodeSessionResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
	gGateNode->HandleNodeRegistrationResponse(*replied);
///<<< END WRITING YOUR CODE

}
