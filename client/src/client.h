#ifndef CLIENT_SRC_CLIENT_H_
#define CLIENT_SRC_CLIENT_H_

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
#include "src/service/service.h"

#include "c2gw.pb.h"

using namespace muduo;
using namespace muduo::net;

google::protobuf::Message* messageToSend;

class PlayerClient : noncopyable
{
public:
    PlayerClient(EventLoop* loop,
        const InetAddress& serverAddr,
        CountDownLatch* allConnected,
        CountDownLatch* allFinished,
        CountDownLatch* all_close)
        : loop_(loop),
        client_(loop, serverAddr, "QueryClient"),
        dispatcher_(std::bind(&PlayerClient::onUnknownMessage, this, _1, _2, _3)),
        codec_(std::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3)),
        all_connected_(allConnected),
        all_finished_(allFinished),
        service_(codec_, all_finished_),
        all_close_(all_close)
    {
        dispatcher_.registerMessageCallback<LoginResponse>(
            std::bind(&ClientService::OnLoginReplied, &service_, _1, _2, _3));
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

    void ReadyGo() // qq tang ready go
    {
        service_.ReadyGo();
    }

private:

    void onConnection(const TcpConnectionPtr& conn)
    {
        if (conn->connected())
        {
            service_.OnConnection(conn);
            all_connected_->countDown();
        }
        else
        {
            all_close_->countDown();
            loop_->quit();
        }
    }

    void onUnknownMessage(const TcpConnectionPtr&,
        const MessagePtr& message,
        Timestamp)
    {
        LOG_INFO << "onUnknownMessage: " << message->GetTypeName();
    }

    EventLoop* loop_{ nullptr };
    TcpClient client_;
    ProtobufDispatcher dispatcher_;
    ProtobufCodec codec_;
    CountDownLatch* all_connected_{ nullptr };
    CountDownLatch* all_finished_{ nullptr };
    ClientService service_;
    CountDownLatch* all_close_{ nullptr };
};
#endif//CLIENT_SRC_CLIENT_H_