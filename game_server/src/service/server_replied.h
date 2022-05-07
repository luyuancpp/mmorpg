#ifndef GAME_SERVER_SRC_MASTER_REPLIED_MS2G_H_
#define GAME_SERVER_SRC_MASTER_REPLIED_MS2G_H_

#include "src/network/rpc_closure.h"

#include "ms_service.pb.h"

#include "logic_proto/scene_rg.pb.h"


class ServerReplied
{
public:

    static ServerReplied& GetSingleton() { thread_local ServerReplied singleton; return singleton; }

    using StartGsMasterRpcClosure = NormalClosure<msservice::StartGsRequest,msservice::StartGsResponse>;
    using StartGsMasterRpcRC = std::shared_ptr<StartGsMasterRpcClosure>;
    void StartGsMasterReplied(StartGsMasterRpcRC cp);

	using StartCrossMainGSClosure = NormalClosure<regionservcie::StartCrossGsRequest, regionservcie::StartCrossGsResponse>;
	using StartCrossGsReplied = std::shared_ptr<StartCrossMainGSClosure>;
	void StartCrossGsRegionReplied(StartCrossGsReplied cp);

private:
};


#endif//GAME_SERVER_SRC_MASTER_REPLIED_MS2G_H_
