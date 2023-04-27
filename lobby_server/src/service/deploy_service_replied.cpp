#include "deploy_service_replied.h"

#include "src/lobby_server.h"

void set_server_squence_node_id(uint32_t node_id);

void OnLobbyServerReplied(const TcpConnectionPtr& conn, const LobbyServerResponsePtr& replied, Timestamp timestamp)
{
    g_lobby_server->StartServer(replied->info());
}

void SceneSqueueNodeIdReplied(const TcpConnectionPtr& conn, const SceneSqueueResponesePtr& replied, Timestamp timestamp)
{
    set_server_squence_node_id(replied->node_id());
}
