#include "replied_dispathcer.h"

#include "src/network/codec/dispatcher.h"

#include "game_service_replied.h"
#include "deploy_service_replied.h"
#include "login_service_replied.h"

extern ProtobufDispatcher g_response_dispatcher;

void InitRepliedCallback()
{
    g_response_dispatcher.registerMessageCallback<gsservice::RpcClientResponse>(std::bind(&OnGsPlayerServiceReplied, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    g_response_dispatcher.registerMessageCallback<deploy::GruoupLoginNodeResponse>(std::bind(&OnLoginNodeInfoReplied, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    g_response_dispatcher.registerMessageCallback<deploy::ServerInfoResponse>(std::bind(&OnServerInfoReplied, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
 
    g_response_dispatcher.registerMessageCallback<LoginNodeCreatePlayerResponse>(std::bind(&OnServerCreatePlayerReplied, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    g_response_dispatcher.registerMessageCallback<LoginNodeEnterGameResponse>(std::bind(&OnEnterGameReplied, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    g_response_dispatcher.registerMessageCallback<LoginNodeLoginResponse>(std::bind(&OnServerLoginReplied, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        
}

