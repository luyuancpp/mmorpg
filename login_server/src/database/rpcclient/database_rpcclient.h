#ifndef LOGIN_SERVER_SRC_DATABASE_RPCCLIENT_DATABASE_RPC_CLIENT_H_
#define LOGIN_SERVER_SRC_DATABASE_RPCCLIENT_DATABASE_RPC_CLIENT_H_

#include "l2db.pb.h"

#include "src/rpc_closure_param/stub_rpc_client.h"

using namespace muduo;
using namespace muduo::net;

namespace login
{
    class DbRpcClient
    {
    public:
        using RpcStub = common::RpcClient<l2db::LoginService_Stub>;
        using RpcClientPtr = std::shared_ptr<RpcStub>;


        static DbRpcClient& GetSingleton()
        {
            static DbRpcClient singleton;
            return singleton;
        }

        template<typename Class, typename MethodParam, typename StubMethod>
        void SendRequest(Class* object,
            void (Class::*method)(MethodParam),
            MethodParam& method_param,
            StubMethod stub_method)
        {
            database_client_->CallMethodString(object, method, method_param, stub_method);
        }

        template<typename MethodParam, typename StubMethod>
        void SendRequest(
            void (method)(MethodParam),
            MethodParam& method_param,
            StubMethod stub_method)
        {
            database_client_->CallMethodString(method, method_param, stub_method);
        }
  
        void Connect(EventLoop* loop,
            const InetAddress& login_server_addr)
        {
            database_client_ = std::make_shared<RpcStub>(loop, login_server_addr);
            database_client_->connect();
        }
    private:
        RpcClientPtr database_client_;
    };

}// namespace login

#endif // LOGIN_SERVER_SRC_DATABASE_RPCCLIENT_DATABASE_RPC_CLIENT_H_

