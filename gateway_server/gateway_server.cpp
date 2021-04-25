#include <stdio.h>

#include "codec/codec.h"
#include "codec/dispatcher.h"

#include "muduo/base/Logging.h"
#include "muduo/base/Mutex.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/TcpServer.h"

#include "src/msg_receiver.h"

using namespace muduo;
using namespace muduo::net;
using namespace gateway;

class GatewayServer : noncopyable
{
public:
    GatewayServer(EventLoop* loop,
        const InetAddress& listenAddr)
        : server_(loop, listenAddr, "QueryServer"),
        dispatcher_(std::bind(&GatewayServer::onUnknownMessage, this, _1, _2, _3)),
        codec_(std::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3)),
        msg_receiver_(codec_)
    {
        dispatcher_.registerMessageCallback<LoginRequest>(
            std::bind(&MsgReceiver::onAnswer, &msg_receiver_, _1, _2, _3));
        server_.setConnectionCallback(
            std::bind(&GatewayServer::onConnection, this, _1));
        server_.setMessageCallback(
            std::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));
    }

    void start()
    {
        server_.start();
    }

private:
    void onConnection(const TcpConnectionPtr& conn)
    {
        LOG_INFO << conn->peerAddress().toIpPort() << " -> "
            << conn->localAddress().toIpPort() << " is "
            << (conn->connected() ? "UP" : "DOWN");
    }

    void onUnknownMessage(const TcpConnectionPtr& conn,
        const MessagePtr& message,
        Timestamp)
    {
        LOG_INFO << "onUnknownMessage: " << message->GetTypeName();
        conn->shutdown();
    }

    TcpServer server_;
    ProtobufDispatcher dispatcher_;
    ProtobufCodec codec_;
    MsgReceiver msg_receiver_;
};

int main(int argc, char* argv[])
{
    LOG_INFO << "pid = " << getpid();
    if (argc > 1)
    {
        EventLoop loop;
        uint16_t port = static_cast<uint16_t>(atoi(argv[1]));
        InetAddress serverAddr(port);
        GatewayServer server(&loop, serverAddr);
        server.start();
        loop.loop();
    }
    else
    {
        printf("Usage: %s port\n", argv[0]);
    }
}

