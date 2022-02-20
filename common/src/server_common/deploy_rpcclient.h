#ifndef COMMON_SRC_NET_DEPLOY_RPCCLIENT_DEPLOY_RPC_CLIENT_H_
#define COMMON_SRC_NET_DEPLOY_RPCCLIENT_DEPLOY_RPC_CLIENT_H_

#include "node2d.pb.h"

#include "src/server_common/rpc_stub.h"

using namespace muduo;
using namespace muduo::net;

namespace deploy
{
    using DeployStub = common::RpcStub<deploy::DeployService_Stub>;
}// namespace deploy

#endif // COMMON_SRC_NET_DEPLOY_RPCCLIENT_DEPLOY_RPC_CLIENT_H_

