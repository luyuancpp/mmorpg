#ifndef GAME_SERVER_SRC_MASTER_REPLIED_MS2G_H_
#define GAME_SERVER_SRC_MASTER_REPLIED_MS2G_H_

#include "src/server_common/rpc_closure.h"

#include "ms_node.pb.h"

namespace ms2gs
{
class RepliedMs2g
{
public:

    static RepliedMs2g& GetSingleton() { thread_local RepliedMs2g singleton; return singleton; }

    using StartGameMasterRpcClosure = common::NormalClosure<msservice::StartGSRequest,
        msservice::StartGSResponse>;
    using StartGameMasterRpcRC = std::shared_ptr<StartGameMasterRpcClosure>;
    void StartGSMasterReplied(StartGameMasterRpcRC cp);
private:
};
}

#endif//GAME_SERVER_SRC_MASTER_REPLIED_MS2G_H_
