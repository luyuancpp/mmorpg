#ifndef GATEWAY_SERVER_SRC_LOGIN_LOGIN_CLIENT_H_
#define GATEWAY_SERVER_SRC_LOGIN_LOGIN_CLIENT_H_

#include "gw2l.pb.h"

#include "muduo/base/noncopyable.h"
#include "muduo/net/EventLoop.h"

#include "src/server_rpc_client/login_client.h"

using namespace muduo;
using namespace muduo::net;

namespace gateway
{
    class LoginRpcClient : muduo::noncopyable
    {
    public:
        using StubClass = gw2l::LoginService_Stub;
        using LoginStub = common::RpcClient<gw2l::LoginService_Stub>;
        using RpcClientPtr = std::shared_ptr<LoginStub>;

        void ConnectLogin(EventLoop* loop,
            const InetAddress& login_server_addr);

        static LoginRpcClient& GetSingleton()
        {
            static LoginRpcClient singleton;
            return singleton;
        }

        template<typename Request, typename Response, typename Class>
        void SendRequest(const Request& request,
            Class* object,
            void (Class::* method)(Response*),
            void (StubClass::* stub_method)(::google::protobuf::RpcController*, const Request*, Response*, ::google::protobuf::Closure*))
        {
            login_client_->SendRequest(request, object, method, stub_method);
        }

        void Replied(gw2l::LoginResponse* response);

    private:
        RpcClientPtr login_client_;
    };
}//namespace gateway

#define  login gateway::LoginRpcClient::GetSingleton() 

#endif // !GATEWAY_SERVER_SRC_LOGIN_LOGIN_CLIENT_H_
