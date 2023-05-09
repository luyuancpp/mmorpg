#include "deploy_service_replied_handler.h"
#include "src/network/codec/dispatcher.h"

///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
extern ProtobufDispatcher g_response_dispatcher;


void InitDeployServiceServerInfoRepliedHandler()
{
	g_response_dispatcher.registerMessageCallback<ServerInfoResponse>(std::bind(&OnDeployServiceServerInfoRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<StartGSResponse>(std::bind(&OnDeployServiceStartGSRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<LobbyServerResponse>(std::bind(&OnDeployServiceStartLobbyServerRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<LobbyServerResponse>(std::bind(&OnDeployServiceAcquireLobbyServerInfoRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<LobbyInfoResponse>(std::bind(&OnDeployServiceAcquireLobbyInfoRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<GruoupLoginNodeResponse>(std::bind(&OnDeployServiceLoginNodeInfoRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
	g_response_dispatcher.registerMessageCallback<SceneSqueueResponese>(std::bind(&OnDeployServiceSceneSequenceNodeIdRepliedHandler, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void OnDeployServiceServerInfoRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<ServerInfoResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnDeployServiceStartGSRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<StartGSResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnDeployServiceStartLobbyServerRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<LobbyServerResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnDeployServiceAcquireLobbyServerInfoRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<LobbyServerResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnDeployServiceAcquireLobbyInfoRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<LobbyInfoResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnDeployServiceLoginNodeInfoRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<GruoupLoginNodeResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnDeployServiceSceneSequenceNodeIdRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<SceneSqueueResponese>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

