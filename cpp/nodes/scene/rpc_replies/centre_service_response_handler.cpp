
#include "centre_service_response_handler.h"

#include "rpc/service_metadata/centre_service_service_metadata.h"
#include "network/codec/message_response_dispatcher.h"

extern MessageResponseDispatcher gRpcResponseDispatcher;

///<<< BEGIN WRITING YOUR CODE
#include "node/system/node/node.h"
///<<< END WRITING YOUR CODE

void InitCentreReply()
{
    gRpcResponseDispatcher.registerMessageCallback<::NodeHandshakeResponse>(CentreNodeHandshakeMessageId,
        std::bind(&OnCentreNodeHandshakeReply, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnCentreNodeHandshakeReply(const TcpConnectionPtr& conn, const std::shared_ptr<::NodeHandshakeResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
	gNode->GetNodeRegistrationManager().OnHandshakeReplied(*replied);
///<<< END WRITING YOUR CODE
}
