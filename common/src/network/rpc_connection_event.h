#pragma once

#include "muduo/net/TcpConnection.h"

#include "src/network/rpc_channel.h"

struct RegisterStubEvent
{
    RegisterStubEvent(const muduo::net::TcpConnectionPtr& conn, muduo::net::RpcChannelPtr& channel)
        : conn_(conn),
            channel_(channel)
    {
    }

    const muduo::net::TcpConnectionPtr& conn_;
    muduo::net::RpcChannelPtr& channel_;

};

struct OnConnected2ServerEvent
{
    OnConnected2ServerEvent(const muduo::net::TcpConnectionPtr& conn)
        : conn_(conn)
    {   
    }
    const muduo::net::TcpConnectionPtr& conn_;
};

struct OnBeConnectedEvent//������������
{
    OnBeConnectedEvent(const muduo::net::TcpConnectionPtr& conn)
        : conn_(conn)
    {
    }
    const muduo::net::TcpConnectionPtr& conn_;
};

