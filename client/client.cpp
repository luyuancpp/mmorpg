#include "codec/dispatcher.h"
#include "codec/codec.h"
#include "pb3.pb.h"

#include "muduo/base/Logging.h"
#include "muduo/base/Mutex.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/TcpClient.h"

#include <stdio.h>


using namespace muduo;
using namespace muduo::net;

typedef std::shared_ptr<Proto3MessageWithMaps> AnswerPtr;

google::protobuf::Message* messageToSend;

class QueryClient : noncopyable
{
public:
    QueryClient(EventLoop* loop,
        const InetAddress& serverAddr)
        : loop_(loop),
        client_(loop, serverAddr, "QueryClient"),
        dispatcher_(std::bind(&QueryClient::onUnknownMessage, this, _1, _2, _3)),
        codec_(std::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3))
    {
        dispatcher_.registerMessageCallback<Proto3MessageWithMaps>(
            std::bind(&QueryClient::onAnswer, this, _1, _2, _3));
        client_.setConnectionCallback(
            std::bind(&QueryClient::onConnection, this, _1));
        client_.setMessageCallback(
            std::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));
    }

    void connect()
    {
        client_.connect();
    }

private:

    void onConnection(const TcpConnectionPtr& conn)
    {
        LOG_INFO << conn->localAddress().toIpPort() << " -> "
            << conn->peerAddress().toIpPort() << " is "
            << (conn->connected() ? "UP" : "DOWN");

        if (conn->connected())
        {
            codec_.send(conn, *messageToSend);
        }
        else
        {
            loop_->quit();
        }
    }

    void onUnknownMessage(const TcpConnectionPtr&,
        const MessagePtr& message,
        Timestamp)
    {
        LOG_INFO << "onUnknownMessage: " << message->GetTypeName();
    }

    void onAnswer(const muduo::net::TcpConnectionPtr&,
        const AnswerPtr& message,
        muduo::Timestamp)
    {
        LOG_INFO << "onAnswer:\n" << message->GetTypeName() << message->DebugString();
    }

    EventLoop* loop_;
    TcpClient client_;
    ProtobufDispatcher dispatcher_;
    ProtobufCodec codec_;
};

int main(int argc, char* argv[])
{
    LOG_INFO << "pid = " << getpid();
    if (argc > 2)
    {
        EventLoop loop;
        uint16_t port = static_cast<uint16_t>(atoi(argv[2]));
        InetAddress serverAddr(argv[1], port);

        Proto3MessageWithMaps query;
       
        Proto3MessageWithMaps empty;
        messageToSend = &query;

        if (argc > 3 && argv[3][0] == 'e')
        {
            messageToSend = &empty;
        }

        QueryClient client(&loop, serverAddr);
        client.connect();
        loop.loop();
    }
    else
    {
        printf("Usage: %s host_ip port [q|e]\n", argv[0]);
    }
}

