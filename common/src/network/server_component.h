#pragma once

#include "muduo/net/TcpConnection.h"

#include "src/network/rpc_channel.h"


struct RpcServerConnection
{
    RpcServerConnection(const muduo::net::TcpConnectionPtr& conn)
        : conn_(conn),
            channel_(boost::any_cast<muduo::net::RpcChannelPtr>(conn->getContext())){}

    bool Connected() const { return conn_->connected(); }

    void Send(const ::google::protobuf::Message& request)
    {
        channel_->S2C(request);
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

static bool IsSameAddr(const muduo::net::InetAddress& conn_addr, const muduo::net::InetAddress& server_info)
{
	return server_info.toIp() == conn_addr.toIp() && server_info.port() == conn_addr.port();
}
