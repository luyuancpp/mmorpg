#include "deploy_service_replied.h"

#include "src/game_config/deploy_json.h"
#include "src/game_server.h"
#include "src/pb/pbc/service_method/deploy_servicemethod.h"

void OnServerInfoReplied(const TcpConnectionPtr& conn, const ServerInfoResponsePtr& replied, Timestamp timestamp)
{
    g_gs->ServerInfo(replied->info());
}

void StartGsDeployReplied(const TcpConnectionPtr& conn, const StartGSResponsePtr& replied, Timestamp timestamp)
{
    g_gs->StartGsDeployReplied(*replied);
}


void OnAcquireLobbyInfoReplied(const TcpConnectionPtr& conn, const LobbyInfoResponsePtr& replied, Timestamp timestamp)
{
    g_gs->OnAcquireLobbyInfoReplied(*replied);
}
