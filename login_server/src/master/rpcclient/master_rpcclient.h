#ifndef LOGIN_SERVER_SRC_MASTER_RPCCLIENT_DATABASE_RPC_CLIENT_H_
#define LOGIN_SERVER_SRC_MASTER_RPCCLIENT_DATABASE_RPC_CLIENT_H_

#ifdef __linux__
#include <unistd.h>
#endif//__linux__

#include "l2ms.pb.h"

#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/protorpc/RpcServer.h"

#include "src/server_rpc_client/login_client.h"

using namespace muduo;
using namespace muduo::net;

namespace login
{
    class MasterRpcClient
    {
    public:
        using RpcStub = common::RpcClient <l2ms::LoginService_Stub>;
        using RpcClientPtr = std::shared_ptr<RpcStub>;

        void Connect(EventLoop* loop,
            const InetAddress& login_server_addr)
        {
            master_client_ = std::make_shared<RpcStub>(loop, login_server_addr);
            master_client_->connect();
        }

        static MasterRpcClient& GetSingleton()
        {
            static MasterRpcClient singleton;
            return singleton;
        }

    private:
        RpcClientPtr master_client_;
    };

}// namespace login

#define  master login::MasterRpcClient::GetSingleton() 

#endif // LOGIN_SERVER_SRC_MASTER_RPCCLIENT_DATABASE_RPC_CLIENT_H_

