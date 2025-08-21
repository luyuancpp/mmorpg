#pragma once
#include <memory>

#include "muduo/base/Logging.h"
#include "muduo/net/InetAddress.h"
#include "muduo/net/TcpClient.h"
#include "muduo/net/TcpConnection.h"

#include "network/game_channel.h"
#include "thread_local/storage.h"
#include "rpc_connection_event.h"
#include <thread_local/dispatcher_manager.h>

using namespace muduo;
using namespace muduo::net;

class RpcClient : noncopyable
{
public:
    RpcClient(EventLoop* loop,
        const InetAddress& serverAddr)
        : client_(loop, serverAddr, "RpcClient"),
        channel_(new GameChannel)
    {
        client_.setConnectionCallback(
            std::bind(&RpcClient::onConnection, this, _1));
        client_.setMessageCallback(
            std::bind(&GameChannel::HandleIncomingMessage, get_pointer(channel_), _1, _2, _3));
        client_.enableRetry();
    }

    const InetAddress& local_addr()const
    {
        if (nullptr == client_.connection())
        {
            static InetAddress s;
            return s;
        }

        return client_.connection()->localAddress();
    }

    const InetAddress& peer_addr()const
    {
        if (nullptr == client_.connection())
        {
            static InetAddress s;
            return s;
        }

        return client_.connection()->peerAddress();
    }

    inline bool connected()const { return connected_; }

    void registerService(google::protobuf::Service* service)
    {
        const google::protobuf::ServiceDescriptor* desc = service->GetDescriptor();
        services_[std::string(desc->full_name())] = service;
    }

    void connect()
    {
        channel_->SetServiceMap(&services_);
        client_.connect();
    }

    void CallRemoteMethod(uint32_t message_id, const ::google::protobuf::Message& request)
    {
        if (!connected_)
        {
            LOG_ERROR << "Failed to call remote method: Client is not connected to the server.";
            return;
        }

        LOG_DEBUG << "Sending request (Message ID: " << message_id << ") to remote method...";
        channel_->CallRemoteMethod(message_id, request);
    }

    void SendRequest(uint32_t message_id, const ::google::protobuf::Message& message)
    {
        if (!connected_)
        {
            LOG_ERROR << "Failed to send request: Client is not connected to the server.";
            return;
        }

        LOG_DEBUG << "Sending request (Message ID: " << message_id << ") with message size: " << message.ByteSizeLong() << " bytes.";
        channel_->SendRequest(message_id, message);
    }

    void RouteMessageToNode(uint32_t message_id, const ::google::protobuf::Message& request)
    {
        if (!connected_)
        {
            LOG_ERROR << "Failed to route message: Client is not connected to the server.";
            return;
        }

        LOG_DEBUG << "Routing message (Message ID: " << message_id << ") to node...";
        channel_->RouteMessageToNode(message_id, request);
    }

private:
    void onConnection(const TcpConnectionPtr& conn)
    {
        if (conn->connected())
        {
            conn->setTcpNoDelay(true);
            channel_->SetConnection(conn);
            connected_ = true;
            LOG_INFO << "Connected to server at " << conn->peerAddress().toIpPort() << ".";
        }
        else
        {
            connected_ = false;
            LOG_WARN << "Disconnected from server at " << conn->peerAddress().toIpPort() << ".";
        }

        dispatcher.trigger<OnConnected2TcpServerEvent>(conn);
    }

    bool connected_{ false };
    TcpClient client_;
    GameChannelPtr channel_;
    std::map<std::string, ::google::protobuf::Service*> services_;
};

using RpcClientPtr = std::shared_ptr<RpcClient>;