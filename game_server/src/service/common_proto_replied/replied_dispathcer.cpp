#include "replied_dispathcer.h"

#include "src/network/codec/dispatcher.h"

#include "controller_service_replied.h"
#include "deploy_service_replied.h"
#include "lobby_service_replied.h"


extern ProtobufDispatcher g_response_dispatcher;

void InitRepliedCallback()
{
    g_response_dispatcher.registerMessageCallback<deploy::ServerInfoResponse>(std::bind(&OnServerInfoReplied, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    g_response_dispatcher.registerMessageCallback<deploy::StartGSResponse>(std::bind(&StartGsDeployReplied, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    g_response_dispatcher.registerMessageCallback<deploy::LobbyInfoResponse>(std::bind(&OnAcquireLobbyInfoReplied, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    g_response_dispatcher.registerMessageCallback<lobbyservcie::StartCrossGsResponse>(std::bind(&OnStartCrossGsReplied, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    
    g_response_dispatcher.registerMessageCallback<controllerservice::StartGsResponse>(std::bind(&StartGsControllerReplied, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    
}

