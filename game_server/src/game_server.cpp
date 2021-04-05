#include "codec/codec.h"
#include "codec/dispatcher.h"
#include "protocal/pb3.pb.h"

#include "muduo/base/Logging.h"
#include "muduo/base/Mutex.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/TcpServer.h"

#include <stdio.h>


using namespace muduo;
using namespace muduo::net;

typedef std::shared_ptr<Proto3MessageWithMaps> QueryPtr;
typedef std::shared_ptr<Proto3MessageWithMaps> AnswerPtr;

class GameServer : noncopyable
{
public:
    GameServer(EventLoop* loop,
        const InetAddress& listenAddr)
        : server_(loop, listenAddr, "QueryServer"),
        dispatcher_(std::bind(&GameServer::onUnknownMessage, this, _1, _2, _3)),
        codec_(std::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3))
    {
        dispatcher_.registerMessageCallback<Proto3MessageWithMaps>(
            std::bind(&GameServer::onQuery, this, _1, _2, _3));
        dispatcher_.registerMessageCallback<Proto3MessageWithMaps>(
            std::bind(&GameServer::onAnswer, this, _1, _2, _3));
        server_.setConnectionCallback(
            std::bind(&GameServer::onConnection, this, _1));
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

    void onQuery(const muduo::net::TcpConnectionPtr& conn,
        const QueryPtr& message,
        muduo::Timestamp)
    {
        LOG_INFO << "onQuery:\n" << message->GetTypeName() << message->DebugString();
        Proto3MessageWithMaps answer;
        codec_.send(conn, answer);

        conn->shutdown();
    }

    void onAnswer(const muduo::net::TcpConnectionPtr& conn,
        const AnswerPtr& message,
        muduo::Timestamp)
    {
        LOG_INFO << "onAnswer: " << message->GetTypeName();
        conn->shutdown();
    }

    TcpServer server_;
    ProtobufDispatcher dispatcher_;
    ProtobufCodec codec_;
};

int main(int argc, char* argv[])
{
    LOG_INFO << "pid = " << getpid();
    if (argc > 1)
    {
        EventLoop loop;
        uint16_t port = static_cast<uint16_t>(atoi(argv[1]));
        InetAddress serverAddr(port);
        GameServer server(&loop, serverAddr);
        server.start();
        loop.loop();
    }
    else
    {
        printf("Usage: %s port\n", argv[0]);
    }
}

