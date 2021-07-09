#ifndef GATEWAY_SERVER_SRC_MASTER_RPCCLIENT_MASTER_RPCCLIENT_H_
#define GATEWAY_SERVER_SRC_MASTER_RPCCLIENT_MASTER_RPCCLIENT_H_

#include "gw2ms.pb.h"

#include "src/server_rpc_client/login_client.h"

using namespace muduo;
using namespace muduo::net;

namespace gateway
{
    class MasterRpcClient
    {
    public:
        using RpcStub = common::RpcClient <gw2ms::LoginService_Stub>;
        using RpcClientPtr = std::shared_ptr<RpcStub>;

        void Connect(EventLoop* loop,
            const InetAddress& login_server_addr)
        {
            gameserver_client_ = std::make_shared<RpcStub>(loop, login_server_addr);
            gameserver_client_->connect();
        }

        static MasterRpcClient& GetSingleton()
        {
            static MasterRpcClient singleton;
            return singleton;
        }

    private:
        RpcClientPtr gameserver_client_;
    };

}// namespace gateway


#endif // GATEWAY_SERVER_SRC_MASTER_RPCCLIENT_MASTER_RPCCLIENT_H_

