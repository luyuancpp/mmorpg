#ifndef GATEWAY_SERVER_SRC_LOGIN_LOGIN_CLIENT_H_
#define GATEWAY_SERVER_SRC_LOGIN_LOGIN_CLIENT_H_

#include "gw2l.pb.h"

#include "src/server_rpc_client/stub_rpc_client.h"

using namespace muduo;
using namespace muduo::net;

namespace gateway
{
    class LoginClient : muduo::noncopyable
    {
    public:
        using StubClass = gw2l::LoginService_Stub;
        using LoginStub = common::RpcClient<gw2l::LoginService_Stub>;
        using RpcClientPtr = std::shared_ptr<LoginStub>;

        void Connect(EventLoop* loop,
            const InetAddress& login_server_addr);

        static LoginClient& s()
        {
            static LoginClient singleton;
            return singleton;
        }

        template<typename Request, typename Response, typename StubMethod>
        void Send(const Request& request,
            void (method)(Response*),
            StubMethod stub_method)
        {
            login_client_->Send<Request, Response, StubMethod>(request, method, stub_method);
        }
    private:
        RpcClientPtr login_client_;
    };
}//namespace gateway

#endif // !GATEWAY_SERVER_SRC_LOGIN_LOGIN_CLIENT_H_
