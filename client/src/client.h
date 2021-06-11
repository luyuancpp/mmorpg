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
#endif//CLIENT_SRC_CLIENT_H_