#ifndef COMMON_SRC_NET_DEPLOY_RPCCLIENT_DEPLOY_RPC_CLIENT_H_
#define COMMON_SRC_NET_DEPLOY_RPCCLIENT_DEPLOY_RPC_CLIENT_H_

#include "deploy.pb.h"

#include "src/rpc_closure_param/rpc_stub.h"

using namespace muduo;
using namespace muduo::net;

namespace deploy
{
    using DeployRpcStub = common::RpcStub<deploy::DeployService_Stub>;
}// namespace deploy

#endif // COMMON_SRC_NET_DEPLOY_RPCCLIENT_DEPLOY_RPC_CLIENT_H_

