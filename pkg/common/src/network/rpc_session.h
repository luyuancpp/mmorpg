#pragma once

#include "muduo/net/TcpConnection.h"

#include "network/game_channel.h"

struct RpcSession
{
	explicit RpcSession(const muduo::net::TcpConnectionPtr& conn)
        : connection(conn),
            channel(boost::any_cast<GameChannelPtr>(conn->getContext())){}

    auto Connected() const -> bool;

    void CallRemoteMethod(const uint32_t message_id, const ::google::protobuf::Message& request) const
	{
		if (!Connected())
		{
			return;
		}
		channel->CallRemoteMethod(message_id, request);
	}

    void SendRequest(const uint32_t message_id, const ::google::protobuf::Message& message) const
    {
		if (!Connected())
		{
			return;
		}
        channel->SendRequest(message_id, message);
    }

    void RouteMessageToNode(const uint32_t message_id, const ::google::protobuf::Message& message) const
    {
		if (!Connected())
		{
			return;
		}
		channel->RouteMessageToNode(message_id, message);
    }

    void SendRouteResponse(const uint32_t message_id,
                           const uint64_t id,
                           const std::string& message_bytes) const
    {
        if (!Connected())
        {
            return;
        }
        channel->SendRouteResponse(message_id, id, message_bytes);
    }

    muduo::net::TcpConnectionPtr connection;
private:
    GameChannelPtr channel;
};

template<typename ServerInfo>
bool IsSameAddr(const muduo::net::InetAddress& conn_addr, const ServerInfo& server_info)
{
	return server_info.ip() == conn_addr.toIp() && server_info.port() == conn_addr.port();
}

bool IsSameAddr(const muduo::net::InetAddress& conn_addr, const muduo::net::InetAddress& server_info);

using RpcSessionPtr = std::shared_ptr<RpcSession>;