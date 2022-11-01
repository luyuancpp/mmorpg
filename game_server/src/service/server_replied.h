#ifndef GAME_SERVER_SRC_MASTER_REPLIED_MS2G_H_
#define GAME_SERVER_SRC_MASTER_REPLIED_MS2G_H_

#include "src/network/rpc_closure.h"

#include "ms_service.pb.h"

#include "logic_proto/scene_rg.pb.h"


class ServerReplied
{
public:

    static ServerReplied& GetSingleton() { thread_local ServerReplied singleton; return singleton; }

    using StartGsMasterRpc = std::shared_ptr< NormalClosure<controllerservice::StartGsRequest, controllerservice::StartGsResponse>>;
    void StartGsMasterReplied(StartGsMasterRpc replied);

	using StartCrossGsRpc = std::shared_ptr<NormalClosure<regionservcie::StartCrossGsRequest, regionservcie::StartCrossGsResponse>>;
	void StartCrossGsRegionReplied(StartCrossGsRpc replied);

private:
};


#endif//GAME_SERVER_SRC_MASTER_REPLIED_MS2G_H_
