
#include "centre_service_replied_handler.h"

#include "service_info//centre_service_service_info.h"
#include "network/codec/message_response_dispatcher.h"

extern MessageResponseDispatcher gResponseDispatcher;


///<<< BEGIN WRITING YOUR CODE
#include "scene/system/game_node_scene_system.h"
#include "proto/common/node.pb.h"
#include "proto/logic/event/server_event.pb.h"
#include "thread_local/storage.h"
#include "scene_node.h"
///<<< END WRITING YOUR CODE



void InitCentreServiceRepliedHandler()
{
    gResponseDispatcher.registerMessageCallback<::Empty>(CentreServiceGatePlayerServiceMessageId,
        std::bind(&OnCentreServiceGatePlayerServiceRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::Empty>(CentreServiceGateSessionDisconnectMessageId,
        std::bind(&OnCentreServiceGateSessionDisconnectRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::LoginResponse>(CentreServiceLoginNodeAccountLoginMessageId,
        std::bind(&OnCentreServiceLoginNodeAccountLoginRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::Empty>(CentreServiceLoginNodeEnterGameMessageId,
        std::bind(&OnCentreServiceLoginNodeEnterGameRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::Empty>(CentreServiceLoginNodeLeaveGameMessageId,
        std::bind(&OnCentreServiceLoginNodeLeaveGameRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::Empty>(CentreServiceLoginNodeSessionDisconnectMessageId,
        std::bind(&OnCentreServiceLoginNodeSessionDisconnectRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::NodeRouteMessageResponse>(CentreServicePlayerServiceMessageId,
        std::bind(&OnCentreServicePlayerServiceRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::Empty>(CentreServiceEnterGsSucceedMessageId,
        std::bind(&OnCentreServiceEnterGsSucceedRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::RouteMessageResponse>(CentreServiceRouteNodeStringMsgMessageId,
        std::bind(&OnCentreServiceRouteNodeStringMsgRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::RoutePlayerMessageResponse>(CentreServiceRoutePlayerStringMsgMessageId,
        std::bind(&OnCentreServiceRoutePlayerStringMsgRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::Empty>(CentreServiceInitSceneNodeMessageId,
        std::bind(&OnCentreServiceInitSceneNodeRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::RegisterNodeSessionResponse>(CentreServiceRegisterNodeSessionMessageId,
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
	gNode->HandleNodeRegistrationResponse(*replied);
///<<< END WRITING YOUR CODE

}
