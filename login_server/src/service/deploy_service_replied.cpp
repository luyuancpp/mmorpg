#include "deploy_service_replied.h"

#include "src/login_server.h"

void OnServerInfoReplied(const TcpConnectionPtr& conn, const ServerInfoResponsePtr& replied, Timestamp timestamp)
{
    g_login_node->StartServer(replied->info());
}
