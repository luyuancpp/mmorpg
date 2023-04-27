#include "replied_dispathcer.h"

#include "src/network/codec/dispatcher.h"

#include "controller_service_replied.h"
#include "deploy_service_replied.h"
#include "lobby_service_replied.h"


extern ProtobufDispatcher g_response_dispatcher;

void InitRepliedCallback()
{
    g_response_dispatcher.registerMessageCallback<ServerInfoResponse>(std::bind(&OnServerInfoReplied, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    g_response_dispatcher.registerMessageCallback<StartGSResponse>(std::bind(&StartGsDeployReplied, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    g_response_dispatcher.registerMessageCallback<LobbyInfoResponse>(std::bind(&OnAcquireLobbyInfoReplied, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    g_response_dispatcher.registerMessageCallback<StartCrossGsResponse>(std::bind(&OnStartCrossGsReplied, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    
    g_response_dispatcher.registerMessageCallback<CtrlStartGsResponse>(std::bind(&StartGsControllerReplied, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    
}

