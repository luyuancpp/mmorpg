#include "deploy_service_replied_handler.h"
#include "src/network/codec/dispatcher.h"

///<<< BEGIN WRITING YOUR CODE
#include "src/game_config/deploy_json.h"
#include "src/game_server.h"
#include "service/deploy_service_service.h"

///<<< END WRITING YOUR CODE
extern ProtobufDispatcher g_response_dispatcher;


void InitDeployServiceGetNodeInfoRepliedHandler()
{
	g_response_dispatcher.registerMessageCallback<NodeInfoResponse>(std::bind(&OnDeployServiceGetNodeInfoRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<StartGsResponse>(std::bind(&OnDeployServiceStartGsRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<GsNodeIdResponse>(std::bind(&OnDeployServiceGetGsNodeIdRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnDeployServiceGetNodeInfoRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<NodeInfoResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
	g_game_node->ServerInfo(replied->info());
///<<< END WRITING YOUR CODE
}

void OnDeployServiceStartGsRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<StartGsResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
	g_game_node->StartGsDeployReplied(*replied);
///<<< END WRITING YOUR CODE
}

void OnDeployServiceGetGsNodeIdRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<GsNodeIdResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
///<<< BEGIN WRITING YOUR CODE
	g_game_node->OnAcquireLobbyInfoReplied(*replied);
///<<< END WRITING YOUR CODE
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
