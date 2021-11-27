#ifndef GAME_SERVER_SRC_MASTER_REPLIED_MS2G_H_
#define GAME_SERVER_SRC_MASTER_REPLIED_MS2G_H_

#include "src/server_common/rpc_closure.h"

#include "g2ms.pb.h"

namespace ms2g
{
class RepliedMs2g
{
public:

    static RepliedMs2g& GetSingleton() { thread_local RepliedMs2g singleton; return singleton; }

    using StartGameMasterRpcClosure = common::RpcClosure<g2ms::StartGameServerRequest,
        g2ms::StartGameServerResponse>;
    using StartGameMasterRpcRC = std::shared_ptr<StartGameMasterRpcClosure>;
    void StartGameServerMasterReplied(StartGameMasterRpcRC cp);
private:
};
}

#endif//GAME_SERVER_SRC_MASTER_REPLIED_MS2G_H_
