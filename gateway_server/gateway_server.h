#ifndef GATEWAY_GATEWAY_SERVER_H_
#define GATEWAY_GATEWAY_SERVER_H_

#include <stdio.h>

#include "src/codec/codec.h"
#include "src/codec/dispatcher.h"

#include "muduo/base/Logging.h"
#include "muduo/base/Mutex.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/TcpServer.h"

#include "src/client/service/service.h"

using namespace muduo;
using namespace muduo::net;

namespace gateway
{

class GatewayServer : noncopyable
{
public:

    GatewayServer(EventLoop* loop,
        const InetAddress& listen_addr)
        : server_(loop, listen_addr, "QueryServer"),
        dispatcher_(std::bind(&GatewayServer::OnUnknownMessage, this, _1, _2, _3)),
        codec_(std::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3)),
        client_receiver_(codec_)
    {
        dispatcher_.registerMessageCallback<LoginRequest>(
            std::bind(&ClientReceiver::OnAnswer, &client_receiver_, _1, _2, _3));
        server_.setConnectionCallback(
            std::bind(&GatewayServer::OnConnection, this, _1));
        server_.setMessageCallback(
            std::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));
    }

    void Start()
    {
        server_.start();
    }

private:
    void OnConnection(const TcpConnectionPtr& conn)
    {
        LOG_INFO << conn->peerAddress().toIpPort() << " -> "
            << conn->localAddress().toIpPort() << " is "
            << (conn->connected() ? "UP" : "DOWN");
    }

    void OnUnknownMessage(const TcpConnectionPtr& conn,
        const MessagePtr& message,
        Timestamp)
    {
        LOG_INFO << "onUnknownMessage: " << message->GetTypeName();
        conn->shutdown();
    }

    TcpServer server_;
    ProtobufDispatcher dispatcher_;
    ProtobufCodec codec_;
    ClientReceiver client_receiver_;
};

} // namespace gateway
 
#endif // !GATEWAY_GATEWAY_SERVER_H_


