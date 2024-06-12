#pragma once

#include "muduo/net/TcpConnection.h"

#include "network/rpc_channel.h"

struct RpcSession
{
    RpcSession(const muduo::net::TcpConnectionPtr& conn)
        : conn_(conn),
            channel_(boost::any_cast<muduo::net::RpcChannelPtr>(conn->getContext())){}

    bool Connected() const { return conn_->connected(); }

	void CallMethod(uint32_t message_id, const ::google::protobuf::Message& request) const
	{
		if (!Connected())
		{
			return;
		}
		channel_->CallMethod(message_id, request);
	}

    void Send(uint32_t message_id, const ::google::protobuf::Message& message) const
    {
		if (!Connected())
		{
			return;
		}
        channel_->Send(message_id, message);
    }

    void Route2Node(uint32_t message_id, const ::google::protobuf::Message& message) const
    {
		if (!Connected())
		{
			return;
		}
		channel_->Route2Node(message_id, message);
    }

    void SendRouteResponse(uint32_t message_id,
                           uint64_t id,
                           const std::string& message_bytes) const
    {
        if (!Connected())
        {
            return;
        }
        channel_->SendRouteResponse(message_id, id, message_bytes);
    }

    muduo::net::TcpConnectionPtr conn_;
private:
    muduo::net::RpcChannelPtr channel_;
};

template<typename ServerInfo>
bool IsSameAddr(const muduo::net::InetAddress& conn_addr, const ServerInfo& server_info)
{
	return server_info.ip() == conn_addr.toIp() && server_info.port() == conn_addr.port();
}

bool IsSameAddr(const muduo::net::InetAddress& conn_addr, const muduo::net::InetAddress& server_info);

using RpcSessionPtr = std::shared_ptr<RpcSession>;