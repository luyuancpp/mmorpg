#ifndef LOGIN_SERVER_SRC_MASTER_RPCCLIENT_DATABASE_RPC_CLIENT_H_
#define LOGIN_SERVER_SRC_MASTER_RPCCLIENT_DATABASE_RPC_CLIENT_H_

#include "l2ms.pb.h"

#include "src/server_rpc_client/stub_rpc_client.h"

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

        template<typename Class, typename MethodParam, typename StubMethod>
        void SendRequest(Class* object,
            void (Class::* method)(MethodParam),
            MethodParam& method_param,
            StubMethod stub_method)
        {
            master_client_->SendRpcString(object, method, method_param, stub_method);
        }
    private:
        RpcClientPtr master_client_;
    };

}// namespace login

#endif // LOGIN_SERVER_SRC_MASTER_RPCCLIENT_DATABASE_RPC_CLIENT_H_

