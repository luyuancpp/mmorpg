#include "deploy_service_replied.h"

#include "src/database_server.h"

void OnServerInfoReplied(const TcpConnectionPtr& conn, const ServerInfoResponsePtr& replied, Timestamp timestamp)
{
    g_database_node->StartServer(replied->info());
}
