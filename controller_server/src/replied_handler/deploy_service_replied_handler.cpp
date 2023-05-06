#include "deploy_service_replied_handler.h"
///<<< BEGIN WRITING YOUR CODE
#include "src/controller_server.h"

void set_server_squence_node_id(uint32_t node_id);
///<<< END WRITING YOUR CODE
void OnServerInfoRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<ServerInfoResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
	g_controller_node->StartServer(replied->info());
///<<< END WRITING YOUR CODE
}

void OnStartGSRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<StartGSResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
	
///<<< END WRITING YOUR CODE
}

void OnStartLobbyServerRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<LobbyServerResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnAcquireLobbyServerInfoRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<LobbyServerResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnAcquireLobbyInfoRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<LobbyInfoResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnLoginNodeInfoRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<GruoupLoginNodeResponse>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
///<<< END WRITING YOUR CODE
}

void OnSceneSqueueNodeIdRepliedHandler(const TcpConnectionPtr& conn, const std::shared_ptr<SceneSqueueResponese>& replied, Timestamp timestamp)
{
///<<< BEGIN WRITING YOUR CODE
	set_server_squence_node_id(replied->node_id());
///<<< END WRITING YOUR CODE
}

