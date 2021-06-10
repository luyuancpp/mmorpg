#include "src/codec/dispatcher.h"
#include "src/codec/codec.h"
#include "c2gw.pb.h"

#include "muduo/base/Logging.h"
#include "muduo/base/Mutex.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/TcpClient.h"

#include <stdio.h>


using namespace muduo;
using namespace muduo::net;

typedef std::shared_ptr<LoginResponse> LoginResponsePtr;

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
        dispatcher_.registerMessageCallback<LoginResponse>(
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
            CurrentThread::sleepUsec(500);
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

    void onAnswer(const muduo::net::TcpConnectionPtr& conn,
        const LoginResponsePtr& message,
        muduo::Timestamp)
    {
        LOG_INFO << "onlogin: " << message->DebugString().c_str();
        //codec_.send(conn, *messageToSend);
    }

    EventLoop* loop_;
    TcpClient client_;
    ProtobufDispatcher dispatcher_;
    ProtobufCodec codec_;
    
};

int main(int argc, char* argv[])
{
    LOG_INFO << "pid = " << getpid();

    EventLoop loop;
    InetAddress serverAddr("127.0.0.1", 2000);

    LoginRequest query;
    query.set_account("luhailong");
    query.set_password("lhl.2020");
    messageToSend = &query;

    QueryClient client(&loop, serverAddr);
    client.connect();
    loop.loop();

    return 0;
}

