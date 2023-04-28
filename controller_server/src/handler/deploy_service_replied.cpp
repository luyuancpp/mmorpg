#include "deploy_service_replied.h"

#include "src/controller_server.h"

void set_server_squence_node_id(uint32_t node_id);

void OnServerInfoReplied(const TcpConnectionPtr& conn, const ServerInfoResponsePtr& replied, Timestamp timestamp)
{
    g_controller_node->StartServer(replied->info());
}

void SceneSqueueNodeIdReplied(const TcpConnectionPtr& conn, const SceneSqueueResponesePtr& replied, Timestamp timestamp)
{
    set_server_squence_node_id(replied->node_id());
}
