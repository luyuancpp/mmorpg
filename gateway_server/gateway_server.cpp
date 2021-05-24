#include <stdio.h>

#include "codec/codec.h"
#include "codec/dispatcher.h"

#include "muduo/base/Logging.h"
#include "muduo/base/Mutex.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/TcpServer.h"

#include "src/clientreceiver/msg_receiver.h"

using namespace muduo;
using namespace muduo::net;
using namespace gateway;

class GatewayServer : noncopyable
{
public:

    GatewayServer(EventLoop* loop,
        const InetAddress& listen_addr)
        : server_(loop, listen_addr, "QueryServer"),
        dispatcher_(std::bind(&GatewayServer::onUnknownMessage, this, _1, _2, _3)),
        codec_(std::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3)),
        client_receiver_(codec_)
    {
        dispatcher_.registerMessageCallback<LoginRequest>(
            std::bind(&MsgReceiver::onAnswer, &client_receiver_, _1, _2, _3));
        server_.setConnectionCallback(
            std::bind(&GatewayServer::onConnection, this, _1));
        server_.setMessageCallback(
            std::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));
    }

    void ConnectLogin(EventLoop* loop,
        const InetAddress& login_server_addr)
    {
        client_receiver_.ConnectLogin(loop, login_server_addr);
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
    MsgReceiver client_receiver_;
};

int main(int argc, char* argv[])
{
    EventLoop loop;

    InetAddress login_server_addr("127.0.0.1", 2001);
    InetAddress server_addr("127.0.0.1", 2000);
    GatewayServer server(&loop, server_addr);
    server.ConnectLogin(&loop, login_server_addr);
    server.start();

    loop.loop();

    return 0;
}

