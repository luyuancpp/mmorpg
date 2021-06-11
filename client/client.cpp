#include <stdio.h>

#include "muduo/base/CountDownLatch.h"
#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/EventLoopThreadPool.h"
#include "muduo/net/InetAddress.h"
#include "muduo/net/TcpClient.h"
#include "muduo/net/TcpConnection.h"
#include "muduo/net/protorpc/RpcChannel.h"

#include "src/codec/dispatcher.h"
#include "src/codec/codec.h"

#include "c2gw.pb.h"


using namespace muduo;
using namespace muduo::net;

typedef std::shared_ptr<LoginResponse> LoginResponsePtr;

google::protobuf::Message* messageToSend;

class PlayerClient : noncopyable
{
public:
    PlayerClient(EventLoop* loop,
        const InetAddress& serverAddr,
        CountDownLatch* allConnected,
        CountDownLatch* allFinished)
        : loop_(loop),
        client_(loop, serverAddr, "QueryClient"),
        dispatcher_(std::bind(&PlayerClient::onUnknownMessage, this, _1, _2, _3)),
        codec_(std::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3)),
        all_connected_(allConnected),
        all_finished_(allFinished)
    {
        dispatcher_.registerMessageCallback<LoginResponse>(
            std::bind(&PlayerClient::OnAnswer, this, _1, _2, _3));
        client_.setConnectionCallback(
            std::bind(&PlayerClient::onConnection, this, _1));
        client_.setMessageCallback(
            std::bind(&ProtobufCodec::onMessage, &codec_, _1, _2, _3));
    }

    void connect()
    {
        client_.enableRetry();
        client_.connect();
    }

    void SendRequest()
    {
        LoginRequest query;
        query.set_account("luhailong11");
        query.set_password("lhl.2021");
        codec_.send(conn_, query);
    }

private:

    void onConnection(const TcpConnectionPtr& conn)
    {
        if (conn->connected())
        {
            conn_ = conn;
            all_connected_->countDown();
        }
        else
        {
            conn_.reset();
            loop_->quit();
        }
    }

    void onUnknownMessage(const TcpConnectionPtr&,
        const MessagePtr& message,
        Timestamp)
    {
        LOG_INFO << "onUnknownMessage: " << message->GetTypeName();
    }

    void OnAnswer(const muduo::net::TcpConnectionPtr& conn,
        const LoginResponsePtr& message,
        muduo::Timestamp)
    {
        LOG_INFO << "login: " << message->DebugString();
        all_finished_->countDown();
    }

    EventLoop* loop_;
    TcpClient client_;
    ProtobufDispatcher dispatcher_;
    ProtobufCodec codec_;
    TcpConnectionPtr conn_;
    CountDownLatch* all_connected_;
    CountDownLatch* all_finished_;
};

int main(int argc, char* argv[])
{
    LOG_INFO << "pid = " << getpid();
    if (argc > 1)
    {
        int nClients = 1;

        if (argc > 2)
        {
            nClients = atoi(argv[2]);
        }

        int nThreads = 1;

        if (argc > 3)
        {
            nThreads = atoi(argv[3]);
        }

        CountDownLatch allConnected(nClients);
        CountDownLatch allFinished(nClients);

        EventLoop loop;
        EventLoopThreadPool pool(&loop, "playerbench-client");
        pool.setThreadNum(nThreads);
        pool.start();
        InetAddress serverAddr("127.0.0.1", 2000);

        std::vector<std::unique_ptr<PlayerClient>> clients;
        for (int i = 0; i < nClients; ++i)
        {
            clients.emplace_back(new PlayerClient(pool.getNextLoop(), serverAddr, &allConnected, &allFinished));
            clients.back()->connect();
        }
        allConnected.wait();
        Timestamp start(Timestamp::now());
        LOG_INFO << "all connected";
        for (int i = 0; i < nClients; ++i)
        {
            clients[i]->SendRequest();
        }
        allFinished.wait();
        Timestamp end(Timestamp::now());
        LOG_INFO << "all finished";
        double seconds = timeDifference(end, start);
        printf("%f seconds\n", seconds);
        printf("%.1f calls per second\n", nClients *  seconds);

        return 0;
    }
    else
    {
        printf("Usage: %s host_ip numClients [numThreads]\n", argv[0]);
    }
    return 0;
}

