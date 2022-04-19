#ifndef GAME_SERVER_SRC_MASTER_REPLIED_MS2G_H_
#define GAME_SERVER_SRC_MASTER_REPLIED_MS2G_H_

#include "src/network/rpc_closure.h"

#include "ms_node.pb.h"

#include "logic_proto/scene_rg.pb.h"


class ServerReplied
{
public:

    static ServerReplied& GetSingleton() { thread_local ServerReplied singleton; return singleton; }

    using StartGameMasterRpcClosure = common::NormalClosure<msservice::StartGSRequest,msservice::StartGSResponse>;
    using StartGameMasterRpcRC = std::shared_ptr<StartGameMasterRpcClosure>;
    void StartGSMasterReplied(StartGameMasterRpcRC cp);

	using StartCrossMainGSClosure = common::NormalClosure<regionservcie::StartCrossMainGSRequest, regionservcie::StartCrossMainGSResponse>;
	using StartCrossMainGSReplied = std::shared_ptr<StartCrossMainGSClosure>;
	void StartCrossMainGSRegionReplied(StartCrossMainGSReplied cp);

	using StartCrossRoomGSClosure = common::NormalClosure<regionservcie::StartCrossRoomGSRequest, regionservcie::StartCrossRoomGSResponse>;
	using StartCrossRoomGSReplied = std::shared_ptr<StartCrossRoomGSClosure>;
	void StartCrossRoomGSRegionReplied(StartCrossRoomGSReplied cp);
private:
};


#endif//GAME_SERVER_SRC_MASTER_REPLIED_MS2G_H_
