#ifndef LOGIN_SERVER_SRC_DATABASE_RPCCLIENT_DATABASE_RPC_CLIENT_H_
#define LOGIN_SERVER_SRC_DATABASE_RPCCLIENT_DATABASE_RPC_CLIENT_H_

#include "l2db.pb.h"

#include "src/rpc_closure_param/stub_rpc_client.h"

using namespace muduo;
using namespace muduo::net;

namespace login
{
    class DbRpcLoginStub : public common::RpcClient<l2db::LoginService_Stub>
    {
    public:
        using LoginStubPtr = std::unique_ptr<DbRpcLoginStub>;

        DbRpcLoginStub(EventLoop* loop,
            const InetAddress& serverAddr)
            : RpcClient(loop, serverAddr)
        {
        }

        static LoginStubPtr& GetSingleton()
        {
            static LoginStubPtr singleton;
            return singleton;
        }

        void Connect(EventLoop* loop,
            const InetAddress& login_server_addr)
        {
            common::RpcClient<l2db::LoginService_Stub>::connect();
        }
    private:
    };

    

}// namespace login

#endif // LOGIN_SERVER_SRC_DATABASE_RPCCLIENT_DATABASE_RPC_CLIENT_H_

