#ifndef COMMON_SRC_NET_DEPLOY_RPCCLIENT_DEPLOY_RPC_CLIENT_H_
#define COMMON_SRC_NET_DEPLOY_RPCCLIENT_DEPLOY_RPC_CLIENT_H_

#include "deploy.pb.h"

#include "src/rpc_closure_param/rpc_stub.h"
#include "src/rpc_closure_param/rpc_stub_client.h"

using namespace muduo;
using namespace muduo::net;

namespace deploy
{
    class DeployRpcClient : noncopyable
    {
    public:
        using RpcClientPtr = std::unique_ptr<common::RpcClient>;

        static RpcClientPtr& GetSingleton()
        {
            static RpcClientPtr singleton;
            return singleton;
        }

        static void Connect(EventLoop* loop,
            const InetAddress& login_server_addr)
        {
            GetSingleton() = std::make_unique<common::RpcClient>(loop, login_server_addr);
            GetSingleton()->connect();
        }
    };

    class ServerInfoRpcStub : noncopyable
    {
    public:
        using RpcStub = common::RpcStub<deploy::DeployService_Stub>;
        static RpcStub& GetSingleton()
        {
            static RpcStub singleton(*DeployRpcClient::GetSingleton());
            return singleton;
        }
    };

}// namespace deploy

#endif // COMMON_SRC_NET_DEPLOY_RPCCLIENT_DEPLOY_RPC_CLIENT_H_

