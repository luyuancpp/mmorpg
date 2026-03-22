#pragma once
#include <memory>

#include "muduo/base/Logging.h"
#include "muduo/net/InetAddress.h"
#include "muduo/net/TcpClient.h"
#include "muduo/net/TcpConnection.h"

#include "network/game_channel.h"

#include "rpc_connection_event.h"
#include <thread_context/dispatcher_manager.h>

class RpcClient : muduo::noncopyable
{
public:
    RpcClient(muduo::net::EventLoop* loop,
        const muduo::net::InetAddress& serverAddr)
        : client_(loop, serverAddr, "RpcClient"),
        channel_(new GameChannel)
    {
        client_.setConnectionCallback(
            std::bind(&RpcClient::onConnection, this, std::placeholders::_1));
        client_.setMessageCallback(
            std::bind(&GameChannel::HandleIncomingMessage, muduo::get_pointer(channel_), std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        client_.enableRetry();
    }

    const muduo::net::InetAddress& local_addr()const
    {
        if (nullptr == client_.connection())
        {
            static muduo::net::InetAddress s;
            return s;
        }

        return client_.connection()->localAddress();
    }

    const muduo::net::InetAddress& peer_addr()const
    {
        if (nullptr == client_.connection())
        {
            static muduo::net::InetAddress s;
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

	muduo::net::TcpConnectionPtr GetConnection() const {
		if (nullptr == client_.connection())
		{
			static muduo::net::TcpConnectionPtr c;
			return c;
		}

		return client_.connection();
	}

private:
    void onConnection(const muduo::net::TcpConnectionPtr& conn)
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
    muduo::net::TcpClient client_;
    GameChannelPtr channel_;
    std::map<std::string, ::google::protobuf::Service*> services_;
};

using RpcClientPtr = std::shared_ptr<RpcClient>;
