#ifndef GATEWAY_SERVER_SRC_msg_receiver
#define GATEWAY_SERVER_SRC_msg_receiver

#include "c2gw.pb.h"
#include "gw2l.pb.h"

#include "codec/codec.h"

#include "muduo/base/noncopyable.h"
#include "muduo/net/EventLoop.h"

#include "src/server_rpc_client/login_client.h"

using namespace muduo;
using namespace muduo::net;

namespace gateway
{
typedef std::shared_ptr<LoginRequest> LoginRequestPtr;

class MsgReceiver : muduo::noncopyable
{
public:
    using LoginStub = RpcClient<gw2l::LoginService_Stub>;
    using RpcClientPtr = std::shared_ptr<LoginStub>;

    MsgReceiver(ProtobufCodec& codec)
        :codec_(codec)
    {}

    void ConnectLogin(EventLoop* loop,
        const InetAddress& login_server_addr);

    //client to gateway 
    void OnAnswer(const muduo::net::TcpConnectionPtr& conn,
        const LoginRequestPtr& message,
        muduo::Timestamp);

    //login to gateway
    void Replied(gw2l::LoginResponse* response);

private:
    ProtobufCodec& codec_;
    RpcClientPtr login_client_;
};
}

#endif // GATEWAY_SERVER_SRC_msg_receiver