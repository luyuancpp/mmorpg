#ifndef GATEWAY_SERVER_SRC_CLENT_SERVICE_SERVICE_H_
#define GATEWAY_SERVER_SRC_CLENT_SERVICE_SERVICE_H_

#include "c2gw.pb.h"

#include "src/codec/codec.h"

#include "muduo/base/noncopyable.h"
#include "muduo/net/EventLoop.h"

#include "src/server_rpc_client/rpc_client_closure.h"
#include "src/server_rpc_client/stub_rpc_client.h"
#include "src/login/login_rpcclient.h"

using namespace muduo;
using namespace muduo::net;

namespace gateway
{
typedef std::shared_ptr<LoginRequest> LoginRequestPtr;

class ClientReceiver : muduo::noncopyable
{
public:
    using LoginStub = common::RpcClient<gw2l::LoginService_Stub>;
    using RpcClientPtr = std::shared_ptr<LoginStub>;

    ClientReceiver(ProtobufCodec& codec)
        :codec_(codec)
    {}


    //client to gateway 
    void OnLogin(const muduo::net::TcpConnectionPtr& conn,
        const LoginRequestPtr& message,
        muduo::Timestamp);

    using LoginCCP = common::ClientClosureParam<LoginResponse, gw2l::LoginRequest, gw2l::LoginResponse>;
    using LoginCCPPtr = std::shared_ptr<LoginCCP>;
    void LoginReplied(LoginCCPPtr cp);

private:
    ProtobufCodec& codec_;

};
}//namespace gateway

#endif // GATEWAY_SERVER_SRC_CLENT_SERVICE_SERVICE_H_