#pragma once

#include <memory>
#include <string>
#include "muduo/base/Logging.h"
#include "muduo/net/TcpConnection.h"
#include "network/game_channel.h"

class RpcSession
{
public:
    explicit RpcSession(const muduo::net::TcpConnectionPtr& conn,
        const std::string& uuid)
		: connection(conn), 
          node_uuid(uuid),
          channel_(boost::any_cast<GameChannelPtr>(conn->getContext())) {}

    [[nodiscard]] bool IsConnected() const
    {
        return connection && connection->connected();
    }

    void CallRemoteMethod(uint32_t messageId, const ::google::protobuf::Message& request) const
    {
        if (!IsConnected()) {
            LOG_ERROR << "Connection is not active. Cannot call remote method.";
            return;
        }
        channel_->CallRemoteMethod(messageId, request);
    }

    void SendRequest(uint32_t messageId, const ::google::protobuf::Message& message) const
    {
        if (!IsConnected()) {
            LOG_ERROR << "Connection is not active. Cannot send request.";
            return;
        }
        channel_->SendRequest(messageId, message);
    }

    void RouteMessageToNode(uint32_t messageId, const ::google::protobuf::Message& message) const
    {
        if (!IsConnected()) {
            LOG_ERROR << "Connection is not active. Cannot route message.";
            return;
        }
        channel_->RouteMessageToNode(messageId, message);
    }

    void SendRouteResponse(uint32_t messageId, uint64_t id, const std::string& messageBytes) const
    {
        if (!IsConnected()) {
            LOG_ERROR << "Connection is not active. Cannot send route response.";
            return;
        }
        channel_->SendRouteResponse(messageId, id, messageBytes);
    }

    muduo::net::TcpConnectionPtr connection;

private:
    GameChannelPtr channel_;
	std::string node_uuid;
	bool handshaked = false;
};



