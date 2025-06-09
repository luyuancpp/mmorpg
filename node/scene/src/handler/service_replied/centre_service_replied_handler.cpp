
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



void InitCentreRepliedHandler()
{
    gResponseDispatcher.registerMessageCallback<::Empty>(CentreGatePlayerServiceMessageId,
        std::bind(&OnCentreGatePlayerServiceRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::Empty>(CentreGateSessionDisconnectMessageId,
        std::bind(&OnCentreGateSessionDisconnectRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::CentreLoginResponse>(CentreLoginNodeAccountLoginMessageId,
        std::bind(&OnCentreLoginNodeAccountLoginRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::Empty>(CentreLoginNodeEnterGameMessageId,
        std::bind(&OnCentreLoginNodeEnterGameRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::Empty>(CentreLoginNodeLeaveGameMessageId,
        std::bind(&OnCentreLoginNodeLeaveGameRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::Empty>(CentreLoginNodeSessionDisconnectMessageId,
        std::bind(&OnCentreLoginNodeSessionDisconnectRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::NodeRouteMessageResponse>(CentrePlayerServiceMessageId,
        std::bind(&OnCentrePlayerServiceRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::Empty>(CentreEnterGsSucceedMessageId,
        std::bind(&OnCentreEnterGsSucceedRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::RouteMessageResponse>(CentreRouteNodeStringMsgMessageId,
        std::bind(&OnCentreRouteNodeStringMsgRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::RoutePlayerMessageResponse>(CentreRoutePlayerStringMsgMessageId,
        std::bind(&OnCentreRoutePlayerStringMsgRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::Empty>(CentreInitSceneNodeMessageId,
        std::bind(&OnCentreInitSceneNodeRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::RegisterNodeSessionResponse>(CentreRegisterNodeSessionMessageId,
        std::bind(&OnCentreRegisterNodeSessionRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnCentreGatePlayerServiceRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnCentreGateSessionDisconnectRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnCentreLoginNodeAccountLoginRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::CentreLoginResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnCentreLoginNodeEnterGameRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnCentreLoginNodeLeaveGameRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnCentreLoginNodeSessionDisconnectRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnCentrePlayerServiceRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::NodeRouteMessageResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnCentreEnterGsSucceedRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnCentreRouteNodeStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::RouteMessageResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnCentreRoutePlayerStringMsgRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::RoutePlayerMessageResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnCentreInitSceneNodeRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnCentreRegisterNodeSessionRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::RegisterNodeSessionResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
    gNode->HandleNodeRegistrationResponse(*replied);
///<<< END WRITING YOUR CODE

}
