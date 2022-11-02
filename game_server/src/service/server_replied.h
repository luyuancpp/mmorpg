#pragma once

#include "src/network/rpc_closure.h"

#include "controller_service.pb.h"

#include "logic_proto/lobby_scene.pb.h"

class ServerReplied
{
public:

    static ServerReplied& GetSingleton() { thread_local ServerReplied singleton; return singleton; }

    using StartGsControllerRpc = std::shared_ptr< NormalClosure<controllerservice::StartGsRequest, controllerservice::StartGsResponse>>;
    void StartGsControllerReplied(StartGsControllerRpc replied);

	using StartCrossGsRpc = std::shared_ptr<NormalClosure<lobbyservcie::StartCrossGsRequest, lobbyservcie::StartCrossGsResponse>>;
	void StartCrossGsReplied(StartCrossGsRpc replied);

private:
};
