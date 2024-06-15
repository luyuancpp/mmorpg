#include "deploy_service_replied_handler.h"
#include "network/codec/dispatcher.h"

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
extern ProtobufDispatcher g_response_dispatcher;


void InitDeployServiceGetNodeInfoRepliedHandler()
{
	g_response_dispatcher.registerMessageCallback<NodeInfoResponse>(std::bind(&OnDeployServiceGetNodeInfoRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnDeployServiceGetNodeInfoRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<NodeInfoResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

