
#include "gate_service_replied_handler.h"

#include "service_info//gate_service_service_info.h"
#include "network/codec/message_response_dispatcher.h"

extern MessageResponseDispatcher gResponseDispatcher;


///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE



void InitGateServiceRepliedHandler()
{
    gResponseDispatcher.registerMessageCallback<::RegisterGameNodeSessionResponse>(GateServicePlayerEnterGameNodeMessageId,
        std::bind(&OnGateServicePlayerEnterGameNodeRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::Empty>(GateServiceSendMessageToPlayerMessageId,
        std::bind(&OnGateServiceSendMessageToPlayerRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::Empty>(GateServiceKickSessionByCentreMessageId,
        std::bind(&OnGateServiceKickSessionByCentreRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::RouteMessageResponse>(GateServiceRouteNodeMessageMessageId,
        std::bind(&OnGateServiceRouteNodeMessageRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::RoutePlayerMessageResponse>(GateServiceRoutePlayerMessageMessageId,
        std::bind(&OnGateServiceRoutePlayerMessageRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::Empty>(GateServiceBroadcastToPlayersMessageId,
        std::bind(&OnGateServiceBroadcastToPlayersRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    gResponseDispatcher.registerMessageCallback<::RegisterNodeSessionResponse>(GateServiceRegisterNodeSessionMessageId,
        std::bind(&OnGateServiceRegisterNodeSessionRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnGateServicePlayerEnterGameNodeRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::RegisterGameNodeSessionResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnGateServiceSendMessageToPlayerRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnGateServiceKickSessionByCentreRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnGateServiceRouteNodeMessageRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::RouteMessageResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnGateServiceRoutePlayerMessageRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::RoutePlayerMessageResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnGateServiceBroadcastToPlayersRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::Empty>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}

void OnGateServiceRegisterNodeSessionRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<::RegisterNodeSessionResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE

}
