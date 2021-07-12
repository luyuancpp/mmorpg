#ifndef LOGIN_SERVER_SRC_DATABASE_RPCCLIENT_DATABASE_RPC_CLIENT_H_
#define LOGIN_SERVER_SRC_DATABASE_RPCCLIENT_DATABASE_RPC_CLIENT_H_

#include "l2db.pb.h"

#include "src/rpc_closure_param/stub_rpc_client.h"

using namespace muduo;
using namespace muduo::net;

namespace login
{
    using LoginStubPtr = std::unique_ptr<common::RpcClient<l2db::LoginService_Stub>>;
    static LoginStubPtr& GetSingletonLoginStub()
    {
        static LoginStubPtr singleton;
        return singleton;
    }

}// namespace login

#endif // LOGIN_SERVER_SRC_DATABASE_RPCCLIENT_DATABASE_RPC_CLIENT_H_

