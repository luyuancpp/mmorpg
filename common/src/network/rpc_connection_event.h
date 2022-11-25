#pragma once

#include "muduo/net/TcpConnection.h"

#include "src/network/rpc_channel.h"


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

