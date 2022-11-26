#include "replied_dispathcer.h"

#include "src/network/codec/dispatcher.h"

#include "game_service_replied.h"
#include "gate_service_replied.h"
#include "deploy_service_replied.h"
#include "lobby_service_replied.h"

extern ProtobufDispatcher g_response_dispatcher;

void InitRepliedCallback()
{
   g_response_dispatcher.registerMessageCallback<deploy::ServerInfoResponse>(std::bind(&OnServerInfoReplied, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
   g_response_dispatcher.registerMessageCallback<deploy::SceneSqueueResponese>(std::bind(&SceneSqueueNodeIdReplied, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

   g_response_dispatcher.registerMessageCallback<NodeServiceMessageResponse>(std::bind(&OnGsCallPlayerReplied, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    
   g_response_dispatcher.registerMessageCallback<lobbyservcie::EnterCrossMainSceneResponese>(std::bind(&OnEnterLobbyMainSceneReplied, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
   
   g_response_dispatcher.registerMessageCallback<gateservice::PlayerEnterGsResponese>(std::bind(&OnGateUpdatePlayerGsReplied, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
   
}

