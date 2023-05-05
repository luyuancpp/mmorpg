#pragma once

#include <stdio.h>

#include "muduo/base/CountDownLatch.h"
#include "muduo/base/Logging.h"
#include "muduo/net/EventLoop.h"
#include "muduo/net/EventLoopThreadPool.h"
#include "muduo/net/InetAddress.h"
#include "muduo/net/TcpClient.h"
#include "muduo/net/TcpConnection.h"
#include "muduo/net/protorpc/RpcChannel.h"

#include "src/network/codec/dispatcher.h"
#include "src/network/codec/codec.h"
#include "src/handler/service.h"

#include "c2gate.pb.h"

using namespace muduo;
using namespace muduo::net;

extern entt::registry::entity_type gAllFinish;

class PlayerClient : noncopyable
{
public:
    PlayerClient(EventLoop* loop,
        const InetAddress& serverAddr)
        : loop_(loop),
        client_(loop, serverAddr, "QueryClient"),
        dispatcher_(std::bind(&PlayerClient::onUnknownMessage, this, _1, _2, _3)),
        codec_(std::bind(&ProtobufDispatcher::onProtobufMessage, &dispatcher_, _1, _2, _3)),
        service_(dispatcher_, codec_, client_)
    {
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

private:

    void onConnection(const TcpConnectionPtr& conn)
    {
        if (conn->connected())
        {
            service_.OnConnection(conn);
            service_.ReadyGo();
        }
        else
        {
            onDisConenction();
        }
     
    }

    void onDisConenction();

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
    ClientService service_;
};

